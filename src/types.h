/**
 * Copyright (c) 2018 by Marek Wydmuch
 * All rights reserved.
 */

#pragma once

#include <cassert>
#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <vector>
#include <queue>

#include "linear.h"
#include "robin_hood.h"

typedef float Weight;
typedef std::pair<int, Weight> SparseWeight;
#define UnorderedMap robin_hood::unordered_flat_map
#define UnorderedSet robin_hood::unordered_flat_set

typedef int Label;
typedef int Example;
typedef feature_node DoubleFeature;
typedef DoubleFeature Feature;

class FileHelper;

struct IntFeature {
    int index;
    int value;

    // Features are sorted by index
    bool operator<(const IntFeature& r) const { return index < r.index; }

    friend std::ostream& operator<<(std::ostream& os, const IntFeature& f) {
        os << f.index << ":" << f.value;
        return os;
    }
};

struct Prediction {
    int label;
    double value; // labels's value/probability/loss

    bool operator<(const Prediction& r) const { return value < r.value; }

    friend std::ostream& operator<<(std::ostream& os, const Prediction& p) {
        os << p.label << ":" << p.value;
        return os;
    }
};

template <typename T> class TopKQueue{
public:
    TopKQueue(){
        k = 0;
    }
    explicit TopKQueue(int k): k(k){};
    ~TopKQueue() = default;

    inline bool empty(){
        return mainQueue.empty();
    }

    inline void push(T x, bool final = false){
        if(k > 0){
            if(final){
                if(finalQueue.size() < k){
                    finalQueue.push(x);
                    mainQueue.push(x);
                } else if(finalQueue.top() < x){
                    finalQueue.pop();
                    finalQueue.push(x);
                    mainQueue.push(x);
                }
            }
            else if(finalQueue.size() < k || finalQueue.top() < x) mainQueue.push(x);
        } else mainQueue.push(x);
    }

    inline void pop(){
        mainQueue.pop();
    }

    inline T top(){
        return mainQueue.top();
    }

private:
    std::priority_queue<T> mainQueue;
    std::priority_queue<T, std::vector<T>, std::greater<>> finalQueue;
    int k;
};

// Simple dense vector
template <typename T> class Vector {
public:
    Vector();
    Vector(size_t s);
    Vector(size_t s, T v);
    ~Vector();

    Vector<T>& operator=(Vector<T> v){
        if (this == &v) return *this;
        v.s = s;
        v.d = new T[s];
        std::copy(d, d + s, v.d);
        return *this;
    }

    void resize(size_t newS);

    // Returns data as T*
    inline T* data() { return d; }

    // Access row also by [] operator
    inline T& operator[](const int index) { return d[index]; }
    inline const T& operator[](const int index) const { return d[index]; }

    // Returns single row size
    inline int size() const { return s; }
    inline unsigned long long mem() { return s * sizeof(T); }

    inline void print(){
        for(int i = 0; i < s; ++i) std::cout << d[i] << " ";
        std::cout << "\n";
    }

    void save(std::ostream& out) const;
    void load(std::istream& in);
private:
    size_t s;   // Size
    T* d;       // Data
};

template <typename T> Vector<T>::Vector() {
    s = 0;
    d = nullptr;
}

template <typename T> Vector<T>::Vector(size_t s): s(s) {
    d = new T[s];
}

template <typename T> Vector<T>::Vector(size_t s, T v): s(s) {
    d = new T[s];
    std::fill(d, d + s, v);
}

template <typename T> Vector<T>::~Vector(){
    delete[] d;
}

template <typename T> void Vector<T>::resize(size_t newS){
    T* newD = new T[newS];
    if(d != nullptr){
        std::copy(d, d + s, newD);
        delete[] d;
    }
    s = newS;
    d = newD;
}

template <typename T> void Vector<T>::save(std::ostream& out) const {
    out.write((char*)&s, sizeof(s));
    out.write((char*)d, s * sizeof(T));
}

template <typename T> void Vector<T>::load(std::istream& in){
    in.read((char*)&s, sizeof(s));
    delete[] d;
    d = new T[s];
    in.read((char*)d, s * sizeof(T));
}


// Simple dense matrix
template <typename T> class Matrix {
public:
    Matrix();
    Matrix(size_t m, size_t n);

    // Access row also by [] operator
    inline Vector<T>& operator[](const int index) { return r[index]; }
    inline const Vector<T>& operator[](const int index) const { return r[index]; }

    // Returns size of matrix
    inline int rows() const { return m; }
    inline int cols() const { return n; }
    inline unsigned long long mem() const { return (m * n) * sizeof(T); }

    void save(std::ostream& out) const;
    void load(std::istream& in);

private:
    size_t m;                      // Row count
    size_t n;                      // Col count
    std::vector<Vector<T>> r;      // Rows
};

template <typename T> Matrix<T>::Matrix() {
    m = 0;
    n = 0;
}

template <typename T> Matrix<T>::Matrix(size_t m, size_t n): m(m), n(n) {
    r.resize(m);
    for(auto& v : r) v.resize(n);
}

template <typename T> void Matrix<T>::save(std::ostream& out) const {
    out.write((char*)&m, sizeof(m));
    out.write((char*)&n, sizeof(n));
    for(const auto& v : r) v.save(out);
}

template <typename T> void Matrix<T>::load(std::istream& in){
    in.read((char*)&m, sizeof(m));
    in.read((char*)&n, sizeof(n));
    r.resize(m);
    for(auto& v : r) v.load(in);
}


// Elastic low-level sparse row matrix, type T needs to contain int at offset 0!
template <typename T> class SRMatrix {
public:
    SRMatrix();
    ~SRMatrix();

    inline void appendRow(const std::vector<T>& row);
    void appendRow(const T* row, const int size);

    inline void replaceRow(const int index, const std::vector<T>& row);
    void replaceRow(const int index, const T* row, const int size);
    
    void appendToRow(const int index, const std::vector<T>& row);
    void appendToRow(const int index, const T* data, const int size = 1);

    // Returns data as T**
    inline T** data() { return r.data(); }
    // inline const T** data() const { return r.data(); }

    // Returns row as T*
    // inline T* row(const int index) { return r[index]; }
    inline T* row(const int index) const { return r[index]; }

    // Returns std::vector<T*>&
    inline std::vector<T*>& allRows() { return r; }

    // Access row also by [] operator
    inline T* operator[](const int index) { return r[index]; }
    inline const T* operator[](const int index) const { return r[index]; }

    // Returns rows' sizes
    inline std::vector<int>& allSizes() { return s; }

    // Returns single row size
    inline int size(const int index) const { return s[index]; }

    // Returns size of matrix
    inline int rows() const { return m; }
    inline int cols() const { return n; }
    inline int cells() const { return c; }

    // Number of cells + -1 cells * size of type + size of vectors
    inline unsigned long long mem() { return (c + n) * sizeof(T) + m * (sizeof(int) + sizeof(T*)); }

    void clear();
    void dump(std::string outfile);
    void save(std::ostream& out);
    void load(std::istream& in);

private:
    int m;              // Row count
    int n;              // Col count
    int c;              // Non zero cells count
    std::vector<int> s; // Rows' sizes
    std::vector<T*> r;  // Rows

    inline T* createNewRow(const T* row, const int size);
    inline void updateN(const T* row, const int size);
};

template <typename T> SRMatrix<T>::SRMatrix() {
    m = 0;
    n = 0;
    c = 0;
}

template <typename T> SRMatrix<T>::~SRMatrix() { clear(); }

template <typename T> inline T* SRMatrix<T>::createNewRow(const T* row, const int size) {
    T* newRow = new T[size + 1];
    std::memcpy(newRow, row, size * sizeof(T));
    std::memset(&newRow[size], -1, sizeof(int)); // Add termination feature (-1)
    return newRow;
}

template <typename T> inline void SRMatrix<T>::updateN(const T* row, const int size) {
    if (size > 0) {
        int rown = *(int*)&row[size - 1] + 1;
        if (n < rown) n = rown;
    }
}

// Data should be sorted
template <typename T> inline void SRMatrix<T>::appendRow(const std::vector<T>& row) {
    appendRow(row.data(), row.size());
}

template <typename T> void SRMatrix<T>::appendRow(const T* row, const int size) {
    s.push_back(size);
    r.push_back(createNewRow(row, size));
    updateN(row, size);
    m = r.size();
    c += size;
}

// Data should be sorted
template <typename T> inline void SRMatrix<T>::replaceRow(const int index, const std::vector<T>& row) {
    replaceRow(index, row.data(), row.size());
}

template <typename T> void SRMatrix<T>::replaceRow(const int index, const T* row, const int size) {
    c += size - s[index];
    s[index] = size;
    delete[] r[index];
    r[index] = createNewRow(row, size);
    updateN(row, size);
}

template <typename T> inline void SRMatrix<T>::appendToRow(const int index, const std::vector<T>& data) {
    appendToRow(index, data.data(), data.size());
}

template <typename T> inline void SRMatrix<T>::appendToRow(const int index, const T* data, const int size) {
    int rSize = s[index];
    T* newRow = new T[s[index] + size + 1];
    std::memcpy(newRow, r[index], rSize * sizeof(T));
    std::memcpy(newRow + rSize, data, size * sizeof(T));
    std::memset(&newRow[size], -1, sizeof(int)); // Add termination feature (-1)
    delete[] r[index];
    r[index] = newRow;
    s[index] += size;
    c += size;
}

template <typename T> void SRMatrix<T>::clear() {
    for (auto row : r) delete[] row;
    r.clear();
    s.clear();

    m = 0;
    n = 0;
    c = 0;
}

template <typename T> void SRMatrix<T>::dump(std::string outfile) {
    std::ofstream out(outfile);

    out << m << " " << n << "\n";
    for (int i = 0; i < m; ++i) {
        for (int j = 0; j < s[i]; ++j) out << r[i][j] << " ";
        out << "\n";
    }

    out.close();
}

template <typename T> void SRMatrix<T>::save(std::ostream& out) {
    out.write((char*)&m, sizeof(m));
    out.write((char*)&n, sizeof(n));

    for (int i = 0; i < m; ++i) {
        out.write((char*)&s[i], sizeof(s[i]));
        for (int j = 0; j <= s[i]; ++j) out.write((char*)&r[i][j], sizeof(T));
    }
}

template <typename T> void SRMatrix<T>::load(std::istream& in) {
    clear();

    in.read((char*)&m, sizeof(m));
    in.read((char*)&n, sizeof(n));

    r.resize(m);
    s.resize(m);

    for (int i = 0; i < m; ++i) {
        int size;
        in.read((char*)&size, sizeof(size));
        T* newRow = new T[size + 1];
        s[i] = size;
        r[i] = newRow;
        c += size;

        for (int j = 0; j <= size; ++j) in.read((char*)&r[i][j], sizeof(T));
    }
}
