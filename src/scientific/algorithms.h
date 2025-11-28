// algorithms.h
//

#ifndef ALGORITHMS_H
#define ALGORITHMS_H

#include <cmath>
#include <limits>
#include <float.h>
#include <stdexcept>
#include <cstring>
#include <cassert>

// ------------------------------------------------------------------//
template <typename T>
void swapElem(T& a, T& b) {
	T t = (a);
	(a) = (b);
	(b) = t;
}

template <typename T>
T kth_smallest(T a[], int n, int k) {
	int i, j, l, m;
	T x;
	l = 0;
	m = n - 1;
	while (l < m) {
		x = a[k];
		i = l;
		j = m;
		do {
			while (a[i] < x) i++;
			while (x < a[j]) j--;
			if (i <= j) {
				swapElem(a[i], a[j]);
				i++;
				j--;
			}
		} while (i <= j);
		if (j < k) l = i;
		if (k < i) m = j;
	}
	return a[k];
}

template <typename T>
T median (T a[], int n) {
	return kth_smallest(a, n, (((n) & 1) ? ((n) / 2) : (((n) / 2) - 1)));
}
template <typename T>
struct LineFit {
	T _slope, _yInt;
	T predict (T x) {
		return _slope * x + _yInt;
	}
	void get_params (T& slope, T& intercept) {
		slope = _slope;
		intercept = _yInt;
	}
	bool fit (const std::valarray<T> &x, const std::valarray<T>& y) {
		int nPoints = x.size ();
		if (nPoints < 2) {
			return false;
		}
		T sumX=0, sumY=0, sumXY=0, sumX2=0;
		for (int i = 0; i < nPoints; i++) {
			sumX += x[i];
			sumY += y[i];
			sumXY += x[i] * y[i];
			sumX2 += x[i] * x[i];
		}
		T xMean = sumX / nPoints;
		T yMean = sumY / nPoints;
		T denominator = sumX2 - sumX * xMean;
		if (std::fabs (denominator) < 1e-7) { // vertical line
			return false;
		}
		_slope = (sumXY - sumX * yMean) / denominator;
		_yInt = yMean - _slope * xMean;
		return true;
	}
};
// ------------------------------------------------------------------//

template <typename T>
void kmeans(T** data, int n, int m, int k, T t, T* labels, T** centroids) {
	int h, i, j;
	T old_error, error = DBL_MAX;
	T **c = centroids;
	T **c1 = (T**) new T*[k];
	int* counts = (int*) new int[k];

	assert(data && k > 0 && k <= n && m > 0 && t >= 0);

	for (h = i = 0; i < k; h += n / k, i++) {
		c1[i] = (T*) calloc(m, sizeof (T));
		if (!centroids) {
			c[i] = (T*) new T[m];
		}
		for (j = m; j-- > 0; c[i][j] = data[h][j]);
	}

	do {
		old_error = error, error = 0;

		for (i = 0; i < k; counts[i++] = 0) {
			for (j = 0; j < m; c1[i][j++] = 0);
		}

		for (h = 0; h < n; h++) {
			T min_distance = DBL_MAX;
			for (i = 0; i < k; i++) {
				T distance = 0;
				for (j = m; j-- > 0; distance += pow(data[h][j] - c[i][j], 2));
				if (distance < min_distance) {
					labels[h] = i;
					min_distance = distance;
				}
			}

			for (j = m; j-- > 0; c1[(int) labels[h]][j] += data[h][j]);
			counts[(int) labels[h]]++;
			error += min_distance;
		}

		for (i = 0; i < k; i++) {
			for (j = 0; j < m; j++) {
				c[i][j] = counts[i] ? c1[i][j] / counts[i] : c1[i][j];
			}
		}

	} while (fabs(error - old_error) > t);


	for (i = 0; i < k; i++) {
		delete [] c1[i];
	}

	delete [] c1;
	delete [] counts;
}

#endif	// ALGORITHMS_H

// EOF
