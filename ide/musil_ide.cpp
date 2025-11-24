// musil_ide.cpp
//
// Musil IDE with FLTK:
//  - Top: text editor for Musil scripts
//  - Middle: single-line "listener" input (REPL)
//  - Bottom: console text display for evaluation output
//  - Draggable splitter between editor and bottom pane (listener+console)
//  - Syntax highlighting (C-like style from FLTK example, can be adapted)
//  - Zoom in/out (View/Zoom In, View/Zoom Out)
//  - Evaluate/Run Script (Cmd+R) and Evaluate/Run Selection (Cmd+E)
//
// Requires: FLTK, musil.h in ../src
//

#include <FL/Fl.H>
#include <FL/Fl_Double_Window.H>
#include <FL/Fl_Menu_Bar.H>
#include <FL/Fl_Text_Editor.H>
#include <FL/Fl_Text_Display.H>
#include <FL/Fl_Text_Buffer.H>
#include <FL/Fl_Native_File_Chooser.H>
#include <FL/Fl_Tile.H>
#include <FL/fl_ask.H>
#include <FL/filename.H>
#include <FL/fl_string_functions.h>
#include <FL/Fl_Input.H>
#include <FL/Fl_Menu_Item.H>

#include <iostream>
#include <sstream>
#include <string>
#include <stdexcept>
#include <cerrno>
#include <cstring>
#include <cstdlib>

#include "musil.h" // your interpreter core

// -----------------------------------------------------------------------------
// Globals (editor/console/listener state)
// -----------------------------------------------------------------------------

Fl_Double_Window *app_window         = nullptr;
Fl_Menu_Bar      *app_menu_bar       = nullptr;

Fl_Tile          *app_tile           = nullptr;
Fl_Text_Editor   *app_editor         = nullptr;
Fl_Text_Buffer   *app_text_buffer    = nullptr;

Fl_Text_Display  *app_console        = nullptr;
Fl_Text_Buffer   *app_console_buffer = nullptr;

class ListenerInput;
ListenerInput    *app_listener       = nullptr;

bool  text_changed = false;
char  app_filename[FL_PATH_MAX] = "";

// Musil environment
AtomPtr musil_env;

// font size for editor/console/listener
int g_font_size = 14;

// For (optional) split editor in highlight code; unused but needed by style code
Fl_Text_Editor *app_split_editor = nullptr;

// Style buffer for syntax highlighting
Fl_Text_Buffer *app_style_buffer = nullptr;

// -----------------------------------------------------------------------------
// Small helper to capture std::cout output
// -----------------------------------------------------------------------------

struct CoutRedirect {
    std::streambuf* old_buf;
    std::ostringstream capture;

    CoutRedirect() {
        old_buf = std::cout.rdbuf(capture.rdbuf());
    }
    ~CoutRedirect() {
        std::cout.rdbuf(old_buf);
    }
    std::string str() const { return capture.str(); }
};

// -----------------------------------------------------------------------------
// Title + filename helpers
// -----------------------------------------------------------------------------

void update_title() {
    const char *fname = nullptr;
    if (app_filename[0])
        fname = fl_filename_name(app_filename);

    if (fname) {
        char buf[FL_PATH_MAX + 3];
        buf[FL_PATH_MAX + 2] = '\0';
        if (text_changed) {
            snprintf(buf, FL_PATH_MAX + 2, "%s *", fname);
        } else {
            snprintf(buf, FL_PATH_MAX + 2, "%s", fname);
        }
        app_window->copy_label(buf);
    } else {
        app_window->label("Musil IDE");
    }
}

void set_changed(bool v) {
    if (v != text_changed) {
        text_changed = v;
        update_title();
    }
}

void set_filename(const char *new_filename) {
    if (new_filename) {
        fl_strlcpy(app_filename, new_filename, FL_PATH_MAX);
    } else {
        app_filename[0] = 0;
    }
    update_title();
}

// -----------------------------------------------------------------------------
// Console helpers
// -----------------------------------------------------------------------------

void console_append(const std::string &s) {
    if (!app_console_buffer) return;
    app_console_buffer->append(s.c_str());
    app_console->insert_position(app_console_buffer->length());
    app_console->show_insert_position();
    app_console->redraw();
}

void console_clear() {
    if (!app_console_buffer) return;
    app_console_buffer->text("");
    app_console->insert_position(0);
    app_console->show_insert_position();
    app_console->redraw();
}

// -----------------------------------------------------------------------------
// Editor change callback
// -----------------------------------------------------------------------------

void text_changed_callback(int, int n_inserted, int n_deleted,
                           int, const char*, void*) {
    if (n_inserted || n_deleted)
        set_changed(true);
}

// -----------------------------------------------------------------------------
// Forward declarations for menu callbacks
// -----------------------------------------------------------------------------

void menu_new_callback(Fl_Widget*, void*);
void menu_open_callback(Fl_Widget*, void*);
void menu_save_callback(Fl_Widget*, void*);
void menu_save_as_callback(Fl_Widget*, void*);
void menu_quit_callback(Fl_Widget*, void*);

void menu_undo_callback(Fl_Widget*, void*);
void menu_redo_callback(Fl_Widget*, void*);
void menu_cut_callback(Fl_Widget*, void*);
void menu_copy_callback(Fl_Widget*, void*);
void menu_paste_callback(Fl_Widget*, void*);
void menu_delete_callback(Fl_Widget*, void*);

void menu_run_script_callback(Fl_Widget*, void*);
void menu_run_selection_callback(Fl_Widget*, void*);

void menu_zoom_in_callback(Fl_Widget*, void*);
void menu_zoom_out_callback(Fl_Widget*, void*);
void menu_syntaxhighlight_callback(Fl_Widget*, void*);

// Listener eval helper
void listener_eval_line();

// -----------------------------------------------------------------------------
// File-related helpers
// -----------------------------------------------------------------------------

void load_file_into_editor(const char *filename) {
    if (app_text_buffer->loadfile(filename) == 0) {
        set_filename(filename);
        set_changed(false);
    } else {
        fl_alert("Failed to load file\n%s\n%s",
                 filename,
                 strerror(errno));
    }
}

void menu_quit_callback(Fl_Widget *, void *) {
    if (text_changed) {
        int r = fl_choice("The current file has not been saved.\n"
                          "Would you like to save it now?",
                          "Cancel", "Save", "Don't Save");
        if (r == 0)   // cancel
            return;
        if (r == 1) { // save
            menu_save_as_callback(nullptr, nullptr);
            return;
        }
        // r == 2 -> don't save
    }
    Fl::hide_all_windows();
}

void menu_new_callback(Fl_Widget*, void*) {
    if (text_changed) {
        int c = fl_choice("Changes in your text have not been saved.\n"
                          "Do you want to start a new text anyway?",
                          "New", "Cancel", nullptr);
        if (c == 1) return;
    }
    app_text_buffer->text("");
    set_filename(nullptr);
    set_changed(false);
}

void menu_open_callback(Fl_Widget*, void*) {
    if (text_changed) {
        int r = fl_choice("The current file has not been saved.\n"
                          "Would you like to save it now?",
                          "Cancel", "Save", "Don't Save");
        if (r == 0) // cancel
            return;
        if (r == 1) // save
            menu_save_as_callback(nullptr, nullptr);
    }

    Fl_Native_File_Chooser file_chooser;
    file_chooser.title("Open File...");
    file_chooser.type(Fl_Native_File_Chooser::BROWSE_FILE);
    if (app_filename[0]) {
        char temp_filename[FL_PATH_MAX];
        fl_strlcpy(temp_filename, app_filename, FL_PATH_MAX);
        const char *name = fl_filename_name(temp_filename);
        if (name) {
            file_chooser.preset_file(name);
            temp_filename[name - temp_filename] = 0;
            file_chooser.directory(temp_filename);
        }
    }
    if (file_chooser.show() == 0) {
        load_file_into_editor(file_chooser.filename());
    }
}

void menu_save_as_callback(Fl_Widget*, void*) {
    Fl_Native_File_Chooser file_chooser;
    file_chooser.title("Save File As...");
    file_chooser.type(Fl_Native_File_Chooser::BROWSE_SAVE_FILE);
    if (app_filename[0]) {
        char temp_filename[FL_PATH_MAX];
        fl_strlcpy(temp_filename, app_filename, FL_PATH_MAX);
        const char *name = fl_filename_name(temp_filename);
        if (name) {
            file_chooser.preset_file(name);
            temp_filename[name - temp_filename] = 0;
            file_chooser.directory(temp_filename);
        }
    }
    if (file_chooser.show() == 0) {
        if (app_text_buffer->savefile(file_chooser.filename()) == 0) {
            set_filename(file_chooser.filename());
            set_changed(false);
        } else {
            fl_alert("Failed to save file\n%s\n%s",
                     file_chooser.filename(),
                     strerror(errno));
        }
    }
}

void menu_save_callback(Fl_Widget*, void*) {
    if (!app_filename[0]) {
        menu_save_as_callback(nullptr, nullptr);
    } else {
        if (app_text_buffer->savefile(app_filename) == 0) {
            set_changed(false);
        } else {
            fl_alert("Failed to save file\n%s\n%s",
                     app_filename,
                     strerror(errno));
        }
    }
}

// -----------------------------------------------------------------------------
// Edit callbacks
// -----------------------------------------------------------------------------

void menu_undo_callback(Fl_Widget*, void*) {
    Fl_Widget *e = Fl::focus();
    if (e && e == app_editor)
        Fl_Text_Editor::kf_undo(0, (Fl_Text_Editor*)e);
}

void menu_redo_callback(Fl_Widget*, void*) {
    Fl_Widget *e = Fl::focus();
    if (e && e == app_editor)
        Fl_Text_Editor::kf_redo(0, (Fl_Text_Editor*)e);
}

void menu_cut_callback(Fl_Widget*, void*) {
    Fl_Widget *e = Fl::focus();
    if (e && e == app_editor)
        Fl_Text_Editor::kf_cut(0, (Fl_Text_Editor*)e);
}

void menu_copy_callback(Fl_Widget*, void*) {
    Fl_Widget *e = Fl::focus();
    if (e && e == app_editor)
        Fl_Text_Editor::kf_copy(0, (Fl_Text_Editor*)e);
}

void menu_paste_callback(Fl_Widget*, void*) {
    Fl_Widget *e = Fl::focus();
    if (e && e == app_editor)
        Fl_Text_Editor::kf_paste(0, (Fl_Text_Editor*)e);
}

void menu_delete_callback(Fl_Widget*, void*) {
    Fl_Widget *e = Fl::focus();
    if (e && e == app_editor)
        Fl_Text_Editor::kf_delete(0, (Fl_Text_Editor*)e);
}

// -----------------------------------------------------------------------------
// Evaluate (Musil) helpers
// -----------------------------------------------------------------------------

void eval_string_in_musil(const std::string &code) {
    CoutRedirect redirect;

    try {
        std::istringstream in(code);
        unsigned linenum = 0;

        while (true) {
            AtomPtr expr = read(in, linenum);
            if (!expr && in.eof()) break;
            if (!expr) continue;

            AtomPtr res = eval(expr, musil_env);

            // REPL-like: print result
            std::ostringstream oss;
            print(res, oss);
            oss << "\n";
            std::cout << oss.str(); // captured
        }
    } catch (AtomPtr &e) {
        std::ostringstream oss;
        oss << "error: ";
        print(e, oss);
        oss << "\n";
        std::cout << oss.str();
    } catch (std::exception &e) {
        std::cout << "exception: " << e.what() << "\n";
    } catch (...) {
        std::cout << "fatal unknown error\n";
    }

    std::string out = redirect.str();
    if (!out.empty())
        console_append(out);
}

void menu_run_script_callback(Fl_Widget*, void*) {
    console_append("[Run script]\n");

    char *text = app_text_buffer->text();
    if (!text) {
        console_append("(empty buffer)\n\n");
        return;
    }

    std::string code(text);
    free(text);

    eval_string_in_musil(code);
    console_append("\n");
}

void menu_run_selection_callback(Fl_Widget*, void*) {
    int start, end;
    if (!app_text_buffer->selection_position(&start, &end)) {
        console_append("[Run selection] no selection; running entire script.\n");
        menu_run_script_callback(nullptr, nullptr);
        return;
    }

    char *sel = app_text_buffer->text_range(start, end);
    if (!sel) {
        console_append("[Run selection] selection empty.\n\n");
        return;
    }

    std::string code(sel);
    free(sel);

    console_append("[Run selection]\n");
    eval_string_in_musil(code);
    console_append("\n");
}

// -----------------------------------------------------------------------------
// Listener input widget
// -----------------------------------------------------------------------------

class ListenerInput : public Fl_Input {
public:
    ListenerInput(int X, int Y, int W, int H, const char* L = 0)
        : Fl_Input(X, Y, W, H, L) {}

    int handle(int ev) override {
        if (ev == FL_KEYDOWN && Fl::event_key() == FL_Enter) {
            listener_eval_line();
            return 1; // we handled it
        }
        return Fl_Input::handle(ev);
    }
};

void listener_eval_line() {
    if (!app_listener) return;
    const char* text = app_listener->value();
    std::string line = text ? text : "";
    if (line.empty()) return;

    app_listener->value("");
    console_append(">> " + line + "\n");
    eval_string_in_musil(line);
    console_append("\n");
}

// -----------------------------------------------------------------------------
// Syntax highlighting (taken from FLTK example, C-like)
// -----------------------------------------------------------------------------

#define TS 14 // default editor textsize for styles

Fl_Text_Display::Style_Table_Entry styletable[] = {
#ifdef TESTING_ATTRIBUTES
  { FL_BLACK,      FL_COURIER,           TS }, // A - Plain
  { FL_DARK_GREEN, FL_HELVETICA_ITALIC,  TS, Fl_Text_Display::ATTR_BGCOLOR,     FL_LIGHT2  }, // B - Line comments
  { FL_DARK_GREEN, FL_HELVETICA_ITALIC,  TS, Fl_Text_Display::ATTR_BGCOLOR_EXT, FL_LIGHT2  }, // C - Block comments
  { FL_BLUE,       FL_COURIER,           TS, Fl_Text_Display::ATTR_UNDERLINE },             // D - Strings
  { FL_DARK_RED,   FL_COURIER,           TS, Fl_Text_Display::ATTR_GRAMMAR },               // E - Directives
  { FL_DARK_RED,   FL_COURIER_BOLD,      TS, Fl_Text_Display::ATTR_STRIKE_THROUGH },        // F - Types
  { FL_BLUE,       FL_COURIER_BOLD,      TS, Fl_Text_Display::ATTR_SPELLING },              // G - Keywords
#else
  { FL_BLACK,      FL_COURIER,           TS }, // A - Plain
  { FL_DARK_GREEN, FL_HELVETICA_ITALIC,  TS }, // B - Line comments
  { FL_DARK_GREEN, FL_HELVETICA_ITALIC,  TS }, // C - Block comments
  { FL_BLUE,       FL_COURIER,           TS }, // D - Strings
  { FL_DARK_RED,   FL_COURIER,           TS }, // E - Directives
  { FL_DARK_RED,   FL_COURIER_BOLD,      TS }, // F - Types
  { FL_BLUE,       FL_COURIER_BOLD,      TS }, // G - Keywords
#endif
};

const int N_STYLES = sizeof(styletable) / sizeof(styletable[0]);

// We keep the original C/C++ keyword/type lists (you can change to Musil keywords)
const char *code_keywords[] = {
  "and", "and_eq", "asm", "bitand", "bitor", "break", "case", "catch", "compl",
  "continue", "default", "delete", "do", "else", "false", "for", "goto", "if",
  "new", "not", "not_eq", "operator", "or", "or_eq", "return", "switch",
  "template", "this", "throw", "true", "try", "while", "xor", "xor_eq"
};

const char *code_types[] = {
  "auto", "bool", "char", "class", "const", "const_cast", "double",
  "dynamic_cast", "enum", "explicit", "extern", "float", "friend", "inline",
  "int", "long", "mutable", "namespace", "private", "protected", "public",
  "register", "short", "signed", "sizeof", "static", "static_cast", "struct",
  "template", "typedef", "typename", "union", "unsigned", "virtual", "void",
  "volatile"
};

extern "C" {
  int compare_keywords(const void *a, const void *b) {
    return strcmp(*((const char **)a), *((const char **)b));
  }
}

// Style parse helpers from FLTK example

void style_parse(const char *text, char *style, int length) {
  char       current;
  int        col;
  int        last;
  char       buf[255], *bufptr;
  const char *temp;

  // Style letters:
  // A - Plain
  // B - Line comments
  // C - Block comments
  // D - Strings
  // E - Directives
  // F - Types
  // G - Keywords

  for (current = *style, col = 0, last = 0; length > 0; length--, text++) {
    if (current == 'B' || current == 'F' || current == 'G') current = 'A';
    if (current == 'A') {
      if (col == 0 && *text == '#') {
        current = 'E';
      } else if (strncmp(text, "//", 2) == 0) {
        current = 'B';
        for (; length > 0 && *text != '\n'; length--, text++) *style++ = 'B';
        if (length == 0) break;
      } else if (strncmp(text, "/*", 2) == 0) {
        current = 'C';
      } else if (strncmp(text, "\\\"", 2) == 0) {
        *style++ = current;
        *style++ = current;
        text++;
        length--;
        col += 2;
        continue;
      } else if (*text == '\"') {
        current = 'D';
      } else if (!last && (islower((*text)&255) || *text == '_')) {
        for (temp = text, bufptr = buf;
             (islower((*temp)&255) || *temp == '_') && bufptr < (buf + sizeof(buf) - 1);
             *bufptr++ = *temp++) {
        }

        if (!islower((*temp)&255) && *temp != '_') {
          *bufptr = '\0';
          bufptr = buf;

          if (bsearch(&bufptr, code_types,
                      sizeof(code_types) / sizeof(code_types[0]),
                      sizeof(code_types[0]), compare_keywords)) {
            while (text < temp) {
              *style++ = 'F';
              text++;
              length--;
              col++;
            }
            text--;
            length++;
            last = 1;
            continue;
          } else if (bsearch(&bufptr, code_keywords,
                             sizeof(code_keywords) / sizeof(code_keywords[0]),
                             sizeof(code_keywords[0]), compare_keywords)) {
            while (text < temp) {
              *style++ = 'G';
              text++;
              length--;
              col++;
            }
            text--;
            length++;
            last = 1;
            continue;
          }
        }
      }
    } else if (current == 'C' && strncmp(text, "*/", 2) == 0) {
      *style++ = current;
      *style++ = current;
      text++;
      length--;
      current = 'A';
      col += 2;
      continue;
    } else if (current == 'D') {
      if (strncmp(text, "\\\"", 2) == 0) {
        *style++ = current;
        *style++ = current;
        text++;
        length--;
        col += 2;
        continue;
      } else if (*text == '\"') {
        *style++ = current;
        col++;
        current = 'A';
        continue;
      }
    }

    if (current == 'A' && (*text == '{' || *text == '}')) *style++ = 'G';
    else *style++ = current;
    col++;

    last = isalnum((*text)&255) || *text == '_' || *text == '.';

    if (*text == '\n') {
      col = 0;
      if (current == 'B' || current == 'E') current = 'A';
    }
  }
}

void style_unfinished_cb(int, void*) {
  // nothing; left for completeness
}

void style_init(void) {
  char *style = new char[app_text_buffer->length() + 1];
  char *text  = app_text_buffer->text();

  memset(style, 'A', app_text_buffer->length());
  style[app_text_buffer->length()] = '\0';

  if (!app_style_buffer)
    app_style_buffer = new Fl_Text_Buffer(app_text_buffer->length());

  style_parse(text, style, app_text_buffer->length());

  app_style_buffer->text(style);
  delete[] style;
  free(text);
}

void style_update(
    int        pos,
    int        nInserted,
    int        nDeleted,
    int        /*nRestyled*/,
    const char * /*deletedText*/,
    void       *cbArg) {

  int   start, end;
  char  last, *style, *text;

  if (nInserted == 0 && nDeleted == 0) {
    app_style_buffer->unselect();
    return;
  }

  if (nInserted > 0) {
    style = new char[nInserted + 1];
    memset(style, 'A', nInserted);
    style[nInserted] = '\0';

    app_style_buffer->replace(pos, pos + nDeleted, style);
    delete[] style;
  } else {
    app_style_buffer->remove(pos, pos + nDeleted);
  }

  app_style_buffer->select(pos, pos + nInserted - nDeleted);

  start = app_text_buffer->line_start(pos);
  end   = app_text_buffer->line_end(pos + nInserted);
  text  = app_text_buffer->text_range(start, end);
  style = app_style_buffer->text_range(start, end);

  if (start == end)
    last = 0;
  else
    last = style[end - start - 1];

  style_parse(text, style, end - start);

  app_style_buffer->replace(start, end, style);
  ((Fl_Text_Editor *)cbArg)->redisplay_range(start, end);

  if (start == end || last != style[end - start - 1]) {
    free(text);
    free(style);

    end   = app_text_buffer->length();
    text  = app_text_buffer->text_range(start, end);
    style = app_style_buffer->text_range(start, end);

    style_parse(text, style, end - start);

    app_style_buffer->replace(start, end, style);
    ((Fl_Text_Editor *)cbArg)->redisplay_range(start, end);
  }

  free(text);
  free(style);
}

void menu_syntaxhighlight_callback(Fl_Widget* w, void*) {
    Fl_Menu_Bar* menu = static_cast<Fl_Menu_Bar*>(w);
    const Fl_Menu_Item* syntaxt_item = menu->mvalue();
    if (!syntaxt_item) return;

    if (syntaxt_item->value()) {
        style_init();
        app_editor->highlight_data(app_style_buffer, styletable,
                                   N_STYLES,
                                   'A', style_unfinished_cb, 0);
        app_text_buffer->add_modify_callback(style_update, app_editor);
    } else {
        app_text_buffer->remove_modify_callback(style_update, app_editor);
        app_editor->highlight_data(nullptr, nullptr, 0, 'A', nullptr, 0);
    }
    app_editor->redraw();
}

// -----------------------------------------------------------------------------
// Zoom helpers
// -----------------------------------------------------------------------------

void apply_font_size() {
    // sync style table sizes too
    for (int i = 0; i < N_STYLES; ++i) {
        styletable[i].size = g_font_size;
    }

    if (app_editor) {
        app_editor->textsize(g_font_size);
        app_editor->redraw();
    }
    if (app_console) {
        app_console->textsize(g_font_size);
        app_console->redraw();
    }
    if (app_listener) {
        app_listener->textsize(g_font_size);
        app_listener->redraw();
    }
}

void menu_zoom_in_callback(Fl_Widget*, void*) {
    g_font_size += 2;
    if (g_font_size > 32) g_font_size = 32;
    apply_font_size();
}

void menu_zoom_out_callback(Fl_Widget*, void*) {
    g_font_size -= 2;
    if (g_font_size < 8) g_font_size = 8;
    apply_font_size();
}

// -----------------------------------------------------------------------------
// Building UI
// -----------------------------------------------------------------------------

void build_app_window() {
    app_window = new Fl_Double_Window(800, 600, "Musil IDE");
}

void build_app_menu_bar() {
    app_window->begin();
    app_menu_bar = new Fl_Menu_Bar(0, 0, app_window->w(), 25);

    // File
    app_menu_bar->add("File/New",         FL_COMMAND + 'n', menu_new_callback);
    app_menu_bar->add("File/Open...",     FL_COMMAND + 'o', menu_open_callback);
    app_menu_bar->add("File/Save",        FL_COMMAND + 's', menu_save_callback);
    app_menu_bar->add("File/Save As...",  FL_COMMAND + 'S', menu_save_as_callback);
    app_menu_bar->add("File/Quit",        FL_COMMAND + 'q', menu_quit_callback);

    // Edit
    app_menu_bar->add("Edit/Undo",   FL_COMMAND + 'z', menu_undo_callback);
    app_menu_bar->add("Edit/Redo",   FL_COMMAND + 'Z', menu_redo_callback, nullptr, FL_MENU_DIVIDER);
    app_menu_bar->add("Edit/Cut",    FL_COMMAND + 'x', menu_cut_callback);
    app_menu_bar->add("Edit/Copy",   FL_COMMAND + 'c', menu_copy_callback);
    app_menu_bar->add("Edit/Paste",  FL_COMMAND + 'v', menu_paste_callback);
    app_menu_bar->add("Edit/Delete", 0,                menu_delete_callback);

    // Evaluate
    app_menu_bar->add("Evaluate/Run Script",     FL_COMMAND + 'r', menu_run_script_callback);
    app_menu_bar->add("Evaluate/Run Selection", FL_COMMAND + 'e', menu_run_selection_callback);

    // View
    app_menu_bar->add("View/Zoom In",           FL_COMMAND + '+', menu_zoom_in_callback);
    app_menu_bar->add("View/Zoom Out",          FL_COMMAND + '-', menu_zoom_out_callback);
    app_menu_bar->add("View/Syntax Highlighting", 0, menu_syntaxhighlight_callback, nullptr, FL_MENU_TOGGLE);

    app_window->callback(menu_quit_callback);
    app_window->end();
}

void build_main_editor_console_listener() {
    int menu_h = app_menu_bar->h();
    int win_w  = app_window->w();
    int win_h  = app_window->h();

    app_window->begin();

    // Text buffer
    app_text_buffer = new Fl_Text_Buffer();
    app_text_buffer->add_modify_callback(text_changed_callback, nullptr);

    // Tile: splits editor (top) and bottom pane (listener+console)
    app_tile = new Fl_Tile(0, menu_h, win_w, win_h - menu_h);

    // Editor: top half (initial)
    int editor_h = (app_tile->h() * 3) / 5; // more space for editor
    app_editor = new Fl_Text_Editor(app_tile->x(), app_tile->y(),
                                    app_tile->w(), editor_h);
    app_editor->buffer(app_text_buffer);
    app_editor->textfont(FL_COURIER);
    app_editor->textsize(g_font_size);
    app_tile->add(app_editor);

    // Bottom group: listener + console
    int bottom_y  = app_editor->y() + app_editor->h();
    int bottom_h  = app_tile->h() - app_editor->h();
    Fl_Group *bottom_group = new Fl_Group(app_tile->x(), bottom_y,
                                          app_tile->w(), bottom_h);

    int bg_x = bottom_group->x();
    int bg_y = bottom_group->y();
    int bg_w = bottom_group->w();
    int bg_h = bottom_group->h();

    int listener_h = 26;

    // Listener input (REPL-style)
    app_listener = new ListenerInput(bg_x, bg_y, bg_w, listener_h);
    app_listener->textfont(FL_COURIER);
    app_listener->textsize(g_font_size);

    // Console display
    app_console_buffer = new Fl_Text_Buffer();
    app_console = new Fl_Text_Display(bg_x, bg_y + listener_h,
                                      bg_w, bg_h - listener_h);
    app_console->buffer(app_console_buffer);
    app_console->textfont(FL_COURIER);
    app_console->textsize(g_font_size);
    app_console->wrap_mode(Fl_Text_Display::WRAP_AT_BOUNDS, 0);

    bottom_group->resizable(app_console);
    bottom_group->end();

    app_tile->add(bottom_group);
    app_tile->resizable(app_editor);
    app_tile->end();

    app_window->resizable(app_tile);
    app_window->end();
    app_tile->init_sizes();
}

// -----------------------------------------------------------------------------
// Musil environment initialization
// -----------------------------------------------------------------------------

void init_musil_env() {
    musil_env = make_env();
    // Best-effort stdlib load; adapt paths as you wish
    try {
        load("stdlib.scm", musil_env);
        console_append("[loaded stdlib.scm]\n");
    } catch (...) {
        console_append("[warning] could not load stdlib.scm]\n");
    }
    console_append("[Musil environment ready]\n\n");
}

// -----------------------------------------------------------------------------
// Main
// -----------------------------------------------------------------------------

int main(int argc, char **argv) {
    try {
        Fl::scheme("oxy");   // set global scheme

        Fl_Preferences prefs(Fl_Preferences::USER, "carminecella", "musil_ide");

        int win_x = 100, win_y = 100, win_w = 800, win_h = 600;
        prefs.get("win_x", win_x, win_x);
        prefs.get("win_y", win_y, win_y);
        prefs.get("win_w", win_w, win_w);
        prefs.get("win_h", win_h, win_h);

        Fl::args_to_utf8(argc, argv);

        build_app_window();
        build_app_menu_bar();
        build_main_editor_console_listener();

        // Load initial file from command line if provided
        if (argc > 1 && argv[1] && argv[1][0] != '-') {
            load_file_into_editor(argv[1]);
        }
        
        app_window->show(argc, argv);
        app_window->resize (win_x, win_y, win_w, win_h);

        // Initialize Musil environment after UI is ready
        init_musil_env();

        return Fl::run();

         // save on exit
        prefs.set("win_x", app_window->x());
        prefs.set("win_y", app_window->y());
        prefs.set("win_w", app_window->w());
        prefs.set("win_h", app_window->h());
    } catch (std::exception &e) {
        fl_alert("Fatal error: %s", e.what());
        return 1;
    } catch (...) {
        fl_alert("Fatal unknown error");
        return 1;
    }
}
