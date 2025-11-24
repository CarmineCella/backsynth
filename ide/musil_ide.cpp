// musil_ide.cpp
//
// Musil IDE with FLTK:
//  - Top: text editor for Musil scripts
//  - Middle: single-line "listener" input (REPL)
//  - Bottom: console text display for evaluation output
//  - Draggable splitter between editor and bottom pane (listener+console)
//  - Musil-oriented syntax highlighting (comments, strings, parens, keywords)
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
#include <cctype>

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
// Musil-oriented syntax highlighting
// -----------------------------------------------------------------------------

// Styles:
//  A - Plain
//  B - Comment   ( ; ... end-of-line )
//  C - String    ( "..." )
//  D - Keyword   (def, lambda, if, ...)
//  E - Paren     ( ( and ) )

Fl_Text_Display::Style_Table_Entry styletable[] = {
    { FL_BLACK,      FL_COURIER,      14 },                 // A - plain
    { FL_DARK_GREEN, FL_COURIER, 14 },                      // B - comments
    { FL_BLUE,       FL_COURIER,      14 },                 // C - strings
    { FL_DARK_RED,   FL_COURIER_BOLD, 14 },                 // D - keywords
    { FL_DARK_BLUE,  FL_COURIER_BOLD, 14 }                  // E - parens
};

const int N_STYLES = sizeof(styletable) / sizeof(styletable[0]);

// Musil-ish keywords (extend as you like)
const char* musil_keywords[] = {
    "=", 
    "%schedule",
    "+",
    "-",
    "*",
    "/",
    "<",
    "<=",
    ">",
    ">=",
    "abs",
    "acos",
    "apply",
    "array",
    "array2list",
    "asin",
    "assign",
    "atan",
    "begin",
    "break",
    "clock",
    "cos",
    "cosh",
    "def",
    "dirlist",
    "eval",
    "exec",
    "exit",
    "exp",
    "filestat",
    "floor",
    "if",
    "info",
    "lambda",
    "lappend",
    "lindex",
    "length",
    "let",
    "list",
    "llength",
    "lrange",
    "lreplace",
    "lset",
    "lshuffle",
    "load",
    "log",
    "log10",
    "macro",
    "max",
    "min",
    "neg",
    "print",
    "read",
    "save",
    "schedule",
    "sin",
    "sinh",
    "size",
    "slice",
    "sleep",
    "sqrt",
    "str",
    "sum",
    "tan",
    "tanh",
    "tostr",
    "udprecv",
    "udpsend",
    "while"
};

const int N_KEYWORDS = sizeof(musil_keywords) / sizeof(musil_keywords[0]);

bool is_ident_start(char c) {
    return std::isalpha((unsigned char)c) || c == '_' || c == '!';
}

bool is_ident_char(char c) {
    // allow typical Scheme-ish chars in symbols
    return std::isalnum((unsigned char)c) ||
           c == '_' || c == '!' || c == '?' ||
           c == '-' || c == '+' || c == '*' ||
           c == '/' || c == '<' || c == '>' || c == '=';
}

bool is_keyword(const std::string& s) {
    for (int i = 0; i < N_KEYWORDS; ++i) {
        if (s == musil_keywords[i]) return true;
    }
    return false;
}

// Very simple Musil lexer -> style buffer
void style_parse_musil(const char* text, char* style, int length) {
    bool in_comment = false;
    bool in_string  = false;

    int i = 0;
    while (i < length) {
        char c = text[i];

        if (in_comment) {
            style[i] = 'B';
            if (c == '\n') {
                in_comment = false;
            }
            ++i;
            continue;
        }

        if (in_string) {
            style[i] = 'C';
            if (c == '"' && (i == 0 || text[i-1] != '\\')) {
                in_string = false;
            }
            ++i;
            continue;
        }

        // Not in comment/string:
        if (c == ';') {
            // start comment until end of line
            in_comment = true;
            style[i] = 'B';
            ++i;
            continue;
        }

        if (c == '"') {
            in_string = true;
            style[i] = 'C';
            ++i;
            continue;
        }

        if (c == '(' || c == ')') {
            style[i] = 'E';
            ++i;
            continue;
        }

        if (is_ident_start(c)) {
            int start = i;
            int j = i + 1;
            while (j < length && is_ident_char(text[j])) j++;
            std::string ident(text + start, text + j);
            char mode = is_keyword(ident) ? 'D' : 'A';
            for (int k = start; k < j; ++k) {
                style[k] = mode;
            }
            i = j;
            continue;
        }

        // default
        style[i] = 'A';
        ++i;
    }
}

// Style initialization: create style buffer for entire text
void style_init() {
    int len = app_text_buffer->length();
    if (len < 0) len = 0;

    char* text  = app_text_buffer->text();
    char* style = new char[len + 1];

    if (!text) {
        std::memset(style, 'A', len);
    } else {
        style_parse_musil(text, style, len);
    }
    style[len] = '\0';

    if (!app_style_buffer) {
        app_style_buffer = new Fl_Text_Buffer(len);
    }
    app_style_buffer->text(style);

    delete [] style;
    if (text) free(text);
}

// Simple style_update: re-parse entire buffer when text changes
void style_update(int, int, int, int, const char*, void* cbArg) {
    style_init();
    int end = app_text_buffer->length();
    ((Fl_Text_Editor*)cbArg)->redisplay_range(0, end);
}

void menu_syntaxhighlight_callback(Fl_Widget* w, void*) {
    Fl_Menu_Bar* menu = static_cast<Fl_Menu_Bar*>(w);
    const Fl_Menu_Item* item = menu->mvalue();
    if (!item) return;

    if (item->value()) {
        style_init();
        app_editor->highlight_data(app_style_buffer, styletable,
                                   N_STYLES,
                                   'A', nullptr, 0);
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

    // Make tile resizable & set ranges so split behaves
    app_tile->resizable(app_editor);
    app_tile->size_range(0, 50, 50); // min height for editor
    app_tile->size_range(1, 50, 50); // min height for bottom group
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

    std::stringstream out;
    out << "[musil, version " << VERSION <<"]" << std::endl <<  std::endl;
    out << "music scripting language" << std::endl;
    out << "(c) " << COPYRIGHT << ", www.carminecella.com" << std::endl << std::endl;

    console_append(out.str ().c_str ());
}

// -----------------------------------------------------------------------------
// Main
// -----------------------------------------------------------------------------

int main(int argc, char **argv) {
    try {
        Fl::args_to_utf8(argc, argv);

        build_app_window();
        build_app_menu_bar();
        build_main_editor_console_listener();

        // Load initial file from command line if provided
        if (argc > 1 && argv[1] && argv[1][0] != '-') {
            load_file_into_editor(argv[1]);
        }

        app_window->show(argc, argv);

        // Initialize Musil environment after UI is ready
        init_musil_env();

        // Turn on syntax highlighting by default
        {
            // mark menu item checked
            Fl_Menu_Item* item = const_cast<Fl_Menu_Item*>(
                app_menu_bar->find_item("View/Syntax Highlighting"));
            if (item) item->set();
            style_init();
            app_editor->highlight_data(app_style_buffer, styletable,
                                       N_STYLES,
                                       'A', nullptr, 0);
            app_text_buffer->add_modify_callback(style_update, app_editor);
        }

        // Apply initial font size to all widgets/styles
        apply_font_size();

        return Fl::run();
    } catch (std::exception &e) {
        fl_alert("Fatal error: %s", e.what());
        return 1;
    } catch (...) {
        fl_alert("Fatal unknown error");
        return 1;
    }
}
