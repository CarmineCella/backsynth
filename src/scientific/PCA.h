#ifndef PCA_H
#define PCA_H

#include <iostream>
#include <cmath>

using namespace std;

template <class T>
T** jabobian(T** input, const int N, const int i, const int j, const T rPhi) {
    T rSp = sin(rPhi);
    T rCp = cos(rPhi);
    T** temp = new T*[N];
    for(int ii = 0; ii < N; ++ii)
        temp[ii] = new T[N];
    for(int ii = 0; ii < N; ++ii)
        for(int jj = 0; jj < N; ++jj) {
            temp[ii][jj] = input[ii][jj];
            input[ii][jj] = (T)0;
        }

    for(long ii = 0; ii < N; ++ii) {
        for(long jj = 0; jj < N; ++jj) {
            if( ii == i ) {  // row i
                if( jj == i ) input[ii][jj] = temp[i][i] * rCp * rCp + temp[j][j] * rSp * rSp + 2 * temp[i][j] * rCp * rSp;
                else if( jj == j ) input[ii][jj] = (temp[j][j] - temp[i][i]) * rSp * rCp + temp[i][j] * (rCp * rCp - rSp * rSp);
                else input[ii][jj] = temp[i][jj] * rCp + temp[j][jj] * rSp;
            } else if ( ii == j ) { // row j
                if( jj == i ) input[ii][jj] = (temp[j][j] - temp[i][i]) * rSp * rCp + temp[i][j] * (rCp * rCp - rSp * rSp);
                else if( jj == j ) input[ii][jj] = temp[i][i] * rSp * rSp + temp[j][j] * rCp * rCp - 2 * temp[i][j] * rCp * rSp;
                else input[ii][jj] = temp[j][jj] * rCp - temp[i][jj] * rSp;
            } else {            // row l ( l!=i,j )
                if( jj == i ) input[ii][jj] = temp[i][ii] * rCp + temp[j][ii] * rSp;
                else if( jj == j ) input[ii][jj] = temp[j][ii] * rCp - temp[i][ii] * rSp;
                else input[ii][jj] = temp[ii][jj];
            }
        }
    }

    for(int ii = 0; ii < N; ++ii) delete [] temp[ii];
    delete [] temp;

    return input;
}

template <class T>
T get_max (T** input, const int N, int &nRow, int &nCol) {
    T rMax = input[0][1];
    nRow = 0;
    nCol = 1;

    for (int i = 0; i < N; ++i) {
        for (int j = 0; j < N; ++j) {
            if (i != j) {
                if( abs(input[i][j]) > rMax ) {
                    rMax = abs(input[i][j]);
                    nRow = i ;
                    nCol = j ;
                }
            }
        }
    }
    return rMax;
}

template <class T>
void PCA(T** Data, T** eigen_vectors, const int M, const int N, const T Err = 0.00001) {
    T*  average     = new T[M] ;
    T** temp_data    = new T*[N]; //[N][M]
    T** S           = new T*[M]; //[M][M]
    T*  eigen_values  = new T[M];
    T** eTemp       = new T*[M]; //[M][M]
    T** eVec        = new T*[M]; //[M][M]
    T** eC          = new T*[M]; //[M][M]


    for(int i = 0; i < N; ++i) {
        temp_data[i] = new T[M];
    }
    for(int i = 0; i < M; ++i) {
        S[i]            = new T[M];
        eTemp[i]        = new T[M];
        eVec[i]         = new T[M];
        eC[i]           = new T[M];
    }

    for(int i = 0; i < M; ++i) {
        average[i] = (T)0;
    }
    for(int i = 0; i < N; ++i) {
        for(int j = 0; j < M; ++j) {
            temp_data[i][j] = Data[i][j];
        }
    }
    for(int i = 0; i < M; ++i) {
        for(int j = 0; j < M; ++j) {
            S[i][j] = eigen_vectors[i][j + 1] = eVec[i][j] = eC[i][j] = (T)0;
        }
    }
    for(int i = 0; i < M; ++i) {
        for(int j = 0; j < M; ++j) {
            eTemp[i][j] = (T)0;
            if(i == j)
                eTemp[i][j] = (T)1;
        }
    }
    for(int i = 0; i < M; ++i) eigen_values[i] = (T)0;

    for(int i = 0; i < M; ++i) {
        for(int j = 0; j < M; ++j) {
            for(int k = 0; k < N; ++k) {
                S[i][j] += temp_data[k][i] * temp_data[k][j] / (T)N;
            }
        }
    }

    while(true) {
        int i = 0, j = 0;
        T rMax = get_max(S, M, i, j);
        if(rMax <= Err) break;

        T rPhi = atan2((T)2 * S[i][j], S[i][i] - S[j][j]) / (T)2;
        S = jabobian(S, M, i, j, rPhi);
        for(int x = 0; x < M; ++x) {
            eC[x][x] = (T)1;
        }
        eC[j][j] = eC[i][i] = cos(rPhi);
        eC[j][i] = sin(rPhi);
        eC[i][j] = -eC[j][i];

        for(int x = 0; x < M; ++x) { // for eigenvectors
            for(int y = 0; y < M; ++y) {
                for(int z = 0; z < M; ++z) {
                    eVec[x][y] = eVec[x][y] + eTemp[x][z] * eC[z][y];
                }
            }
        }

        for(int x = 0; x < M; ++x) {
            for(int y = 0; y < M; ++y) {
                eTemp[x][y] = eVec[x][y];
                eVec[x][y]  = (T)0;
                eC[x][y]    = (T)0;
            }
        }
    }

    for(int i = 0; i < M; ++i) eigen_values[i] = eigen_vectors[i][M] = S[i][i];
    for(int i = 0; i < M; ++i) {
        for(int j = 0; j < M; ++j) {
            eigen_vectors[i][j] = eTemp[j][i];
        }
    }

    T* temp_eigen_vectors = new T[M];

    for(int i = 0; i < M; ++i) {
        for(int j = i; j < M - 1; ++j) {
            if(eigen_values[j] < eigen_values[j + 1]) {
                T TempEigenValue = eigen_values[j + 1];
                eigen_values[j + 1] = eigen_vectors[j + 1][M] = eigen_values[j];
                eigen_values[j]  = eigen_vectors[j][M]  = TempEigenValue;
                for(int k = 0; k < M; ++k) temp_eigen_vectors[k]  = eigen_vectors[j + 1][k];
                for(int k = 0; k < M; ++k) eigen_vectors[j + 1][k] = eigen_vectors[j][k];
                for(int k = 0; k < M; ++k)eigen_vectors[j][k]   = temp_eigen_vectors[k];
            }
        }
    }

    delete [] temp_eigen_vectors;
    delete [] average ;
    delete [] eigen_values ;

    for(int i = 0; i < N; ++i) delete [] temp_data[i];
    delete [] temp_data;

    for(int i = 0; i < M; ++i) {
        delete [] S[i];
        delete [] eTemp[i];
        delete [] eVec[i];
        delete [] eC[i];
    }
    delete [] S;
    delete [] eTemp;
    delete [] eVec;
    delete [] eC;
}

#endif // PCA_H
