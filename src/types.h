/**
 * Copyright (c) 2018 by Marek Wydmuch
 * All rights reserved.
 */

#pragma once

#include <cstdlib>
#include <cstddef>
#include <cassert>
#include <cstring>
#include <vector>
#include "linear.h"

typedef int Label;
typedef feature_node Feature;

// Elastic sparse row matrix, type T needs to contain int at offset 0!
template <typename T>
class SRMatrix {
public:
    SRMatrix();
    ~SRMatrix();
    void appendRow(const std::vector<T>& row);
    void appendRow(const T* row, const int size);

    // Returns data as T**
    inline T** data(){ return r.data(); }

    // Returns rows' sizes
    inline std::vector<int>& sizes(){ return s; }
    inline int rows(){ return m; }
    inline int cols(){ return n; }

private:
    int m; // Row count
    int n; // Col count
    std::vector<int> s; // Rows' sizes
    std::vector<T*> r; // Rows
};

template <typename T>
SRMatrix<T>::SRMatrix(){
    m = 0;
    n = 0;
}

template <typename T>
SRMatrix<T>::~SRMatrix(){
    for(auto row : r) delete[] row;
}

// Data should be sorted
template <typename T>
void SRMatrix<T>::appendRow(const std::vector<T>& row){
    appendRow(row.data(), row.size());
}

template <typename T>
void SRMatrix<T>::appendRow(const T* row, const int size){
    s.push_back(size);

    T *newRow = new T[size + 1];
    std::memcpy(newRow, row, size * sizeof(T));
    std::memset(&newRow[size], -1, sizeof(T)); // Add termination feature (-1)
    r.push_back(newRow);

    if(size > 0){
        int rown = *(int *)&row[size - 1] + 1;
        if(n < rown) n = rown;
    }

    m = r.size();
}
