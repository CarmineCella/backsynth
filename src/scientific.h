// scientific.h
//

#include "core.h"
#include "scientific/algorithms.h"
#include "scientific/KNN.h"
#include "scientific/Matrix.h"
#include "scientific/PCA.h"

#include <valarray>
#include <string>
#include <sstream>
#include <iomanip>

AtomPtr matrix2list (Matrix<Real>& m) {
	AtomPtr l = make_atom ();
	for (std::size_t row = 0; row < m.rows (); ++row) {
		std::valarray<Real> rarray (m.cols ());
		for (std::size_t col = 0; col < m.cols (); ++col) {
			rarray[col] = m (row, col);
		}
		l->tail.push_back (make_atom (rarray));
	}
	return l;
}
Matrix<Real> list2matrix (AtomPtr l) {
	Matrix<Real> m (l->tail.size (), type_check (l->tail.at (0), ARRAY)->array.size ());
	for (unsigned i = 0; i < l->tail.size (); ++i) {
		std::valarray<Real>& row = type_check (l->tail.at (i), ARRAY)->array;
		for (unsigned j = 0; j < row.size (); ++j) {
			m (i, j) = row[j];
		}	
	}
	return m;
}
AtomPtr fn_matdisp (AtomPtr node, AtomPtr env) {
	for (unsigned i = 0; i < node->tail.size (); ++i) {
		AtomPtr lmatrix = type_check (node->tail.at (i), LIST);
		int n = lmatrix->tail.size (); // rows
		if (n < 1) error ("[matdisp] invalid matrix size", node);
		Matrix<Real> m = list2matrix (lmatrix);
		m.print (std::cout) << std::endl;
	}
	return make_atom ("");
}
AtomPtr fn_matmul (AtomPtr node, AtomPtr env) {
	Matrix<Real> a = list2matrix (type_check (node->tail.at (0), LIST));
	for (unsigned i = 1; i < node->tail.size (); ++i) {
		Matrix<Real> b = list2matrix (type_check (node->tail.at (i), LIST));
		if (a.cols () != b.rows ()) error ("[matmul] nonconformant arguments", node);
		a = a * b;
	}
	return matrix2list (a);
}
AtomPtr fn_matsum (AtomPtr node, AtomPtr env) {
	Matrix<Real> a = list2matrix (type_check (node->tail.at (0), LIST));
	int axis = (int) type_check (node->tail.at (1), ARRAY)->array[0];
	Matrix<Real> b = a.sum (axis);
	return matrix2list (b);
}	
AtomPtr fn_rows (AtomPtr node, AtomPtr env) {
	Matrix<Real> a = list2matrix (type_check (node->tail.at (0), LIST));
	return make_atom (a.rows ());
}		
AtomPtr fn_cols (AtomPtr node, AtomPtr env) {
	Matrix<Real> a = list2matrix (type_check (node->tail.at (0), LIST));
	return make_atom (a.cols ());
}		
AtomPtr fn_mattran (AtomPtr node, AtomPtr env) {
	Matrix<Real> a = list2matrix (type_check (node->tail.at (0), LIST));
	Matrix<Real> tr = a.transpose ();
	return matrix2list (tr);
}	
template <int MODE>
AtomPtr fn_matget (AtomPtr node, AtomPtr env) {
	Matrix<Real> a = list2matrix (type_check (node->tail.at (0), LIST));
	int start = (int) type_check (node->tail.at (1), ARRAY)->array[0];
	int end = (int) type_check (node->tail.at (2), ARRAY)->array[0];
	Matrix<Real> b (1, 1);
	if (MODE == 0) {
		if (start < 0 || start >= a.rows () || end < 0 || end >= a.rows ()) error ("[getrows] invalid row selection", node);
		b = a.get_rows (start, end);
	}
	if (MODE == 1) {
		if (start < 0 || start >= a.cols () || end < 0 || end >= a.cols ()) error ("[getcols] invalid col selection", node);
		b = a.get_cols (start, end);
	}
	return matrix2list (b);
}		
AtomPtr fn_eye (AtomPtr node, AtomPtr env) {
	int n = (int) type_check (node->tail.at (0), ARRAY)->array[0];
	Matrix<Real> e (n, n);
	e.id ();
	return matrix2list (e);
}		
AtomPtr fn_inv (AtomPtr node, AtomPtr env) {
	Matrix<Real> a = list2matrix (type_check (node->tail.at (0), LIST));
	Matrix<Real> tr = a.inverse ();
	return matrix2list (tr);
}		
AtomPtr fn_det (AtomPtr node, AtomPtr env) {
	Matrix<Real> a = list2matrix (type_check (node->tail.at (0), LIST));
	return make_atom (a.det ());
}		
AtomPtr fn_median (AtomPtr node, AtomPtr env) {
	std::valarray<Real>& v = type_check (node->tail.at (0), ARRAY)->array;
	int order = (int) type_check (node->tail.at (1), ARRAY)->array[0];

	if (order < 0 || order >= v.size ()) error ("[median] invalid order", node);

	Real init = 0;
	std::valarray<Real> in (init, v.size () + order);
	in[std::slice(order / 2, v.size () + order / 2, 1)] = v;
	std::valarray<Real> out (init, v.size () + order);
	for (unsigned i = 0; i < v.size (); ++i) {
		std::valarray<Real> s = in[std::slice (i, i + order, 1)];
		out[i] = median<Real> (&s[0], order); // alignment is order / 2
	}
	std::valarray<Real> c = out[std::slice(0, v.size (), 1)];
	return make_atom(c);
}
AtomPtr fn_linefit (AtomPtr node, AtomPtr env) {
	std::valarray<Real>& x = type_check (node->tail.at(0), ARRAY)->array;
	std::valarray<Real>& y = type_check (node->tail.at(1), ARRAY)->array;
	if (x.size () != y.size ()) error ("[linefit] x and y must have the same size", node);
	LineFit<Real> line;
	if (!line.fit (x, y)) error ("[linefit] cannot fit a vertical line", node);
	Real slope = 0, intercept = 0;
	line.get_params (slope, intercept);
	std::valarray<Real> l ({slope, intercept});
	return make_atom (l);
}		
AtomPtr fn_pca (AtomPtr node, AtomPtr env) {
	Matrix<Real> a = list2matrix (type_check (node->tail.at (0), LIST));
	Matrix<Real> eigm (a.cols (), a.cols () + 1, 0); // extra col for eigenvals
	PCA<Real> (a.data (), eigm.data (), a.cols (), a.rows ());
	return matrix2list (eigm);
}	
// AtomPtr fn_knntrain (AtomPtr node, AtomPtr env) {
// 	AtomPtr data = type_check (node->tail.at (0), LIST);
// 	int k = type_check (node->tail.at (1), ARRAY)->array[0];
// 	int obs = data->tail.size ();
// 	if (obs < 1) error ("[knntrain] insufficient number of observations", node);
// 	if (k < 1 || k > obs) error ("[knntrain] invalid K parameter", node);
// 	int features = data->tail.at (0)->tail.at (0)->array.size (); // take the first sample for dimensions
// 	if (features < 1) error ("[knntrain] invalid number of features", node);

// 	KNN<Real>* knn = new KNN<Real>(k, features);
// 	for (unsigned i = 0; i < data->tail.size (); ++i) {
// 		Observation<Real>* o = new Observation<Real> ();
// 		AtomPtr item = type_check (data->tail.at (i), LIST);
// 		if (item->tail.size () != 2) error ("[knntrain] malformed item", node);
// 		o->attributes = type_check (item->tail.at (0), ARRAY)->array;
// 		std::stringstream s;
// 		puts (item->tail.at (1), s);
// 		o->classlabel = s.str ();
// 		knn->addObservation (o);
// 	} 
// 	return make_obj ("knntrain", (void*) knn, make_atom ()); 
// }	
// AtomPtr fn_knntest (AtomPtr node, AtomPtr env) {
// 	AtomPtr p = type_check (node->tail.at(0), OBJECT);
//     if (p->obj == 0 || p->lexeme != "knntrain") return  make_atom(0);
//     KNN<Real>* knn = static_cast<KNN<Real>*>(p->obj);
// 	AtomPtr classif = make_atom ();
// 	AtomPtr data = type_check (node->tail.at(1), LIST);
// 	for (unsigned i = 0; i < data->tail.size (); ++i) {
// 		AtomPtr item =  type_check (data->tail.at(i), LIST);
// 		Observation<Real> o;
// 		o.attributes = item->tail.at (0)->array;
// 		classif->tail.push_back (make_atom (knn->classify (o)));
// 	}
// 	return classif;
// }	
AtomPtr fn_kmeans (AtomPtr node, AtomPtr env) {
	Matrix<Real> a = list2matrix (type_check (node->tail.at (0), LIST));
	int K = (int) type_check (node->tail.at (1), ARRAY)->array[0];
	Matrix<Real> centroids (K, a.cols ()); 
	// void kmeans(T** data, int n, int m, int k, T t, int* labels, T** centroids)
	std::valarray<Real> labels (a.rows ());
	kmeans<Real> (a.data (), a.rows (), a.cols (), K, .00001,  &labels[0], centroids.data ());
	AtomPtr res = make_atom ();
	res->tail.push_back (make_atom (labels));
	res->tail.push_back (matrix2list (centroids));
	return res;
}		
AtomPtr add_scientific (AtomPtr env) {
	add_op ("matdisp", fn_matdisp, 1, env);
	add_op ("matmul", fn_matmul, 2, env);
	add_op ("matsum", fn_matsum, 2, env);
	add_op ("rows", fn_rows, 1, env);
	add_op ("cols", fn_cols, 1, env);
	add_op ("getrows", fn_matget<0>, 3, env);
	add_op ("getcols", fn_matget<1>, 3, env);
	add_op ("transp", fn_mattran, 1, env);
	add_op ("eye", fn_eye, 1, env);
	add_op ("inv", fn_inv, 1, env);
	add_op ("det", fn_det, 1, env);
	add_op ("median", fn_median, 2, env);
	add_op ("linefit", fn_linefit, 2, env);
	add_op ("pca", fn_pca, 1, env);
	// add_op ("knntrain", fn_knntrain, 2, env);
	// add_op ("knntest", fn_knntest, 2, env);
	add_op ("kmeans", fn_kmeans, 2, env);
	return env;
}

// eof
