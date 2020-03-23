/**
 * Copyright (c) 2018 by Marek Wydmuch
 * All rights reserved.
 */


#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <mutex>

#include "misc.h"
#include "threads.h"


// Data utils
std::vector<Prediction> computeLabelsPriors(const SRMatrix<Label>& labels) {
    std::cerr << "Computing labels' prior probabilities ...\n";

    std::vector<Prediction> labelsProb;

    labelsProb.resize(labels.cols());
    for (int i = 0; i < labelsProb.size(); ++i) {
        labelsProb[i].label = i;
        labelsProb[i].value = 0;
    }
    int rows = labels.rows();

    for (int r = 0; r < rows; ++r) {
        printProgress(r, rows);
        int rSize = labels.size(r);
        auto rLabels = labels[r];
        for (int i = 0; i < rSize; ++i) ++labelsProb[rLabels[i]].value;
    }

    for (auto& p : labelsProb) p.value /= labels.rows();

    return labelsProb;
}

void computeLabelsFeaturesMatrixThread(std::vector<UnorderedMap<int, double>>& tmpLabelsFeatures,
                                       const SRMatrix<Label>& labels, const SRMatrix<Feature>& features,
                                       bool weightedFeatures, int threadId, int threads,
                                       std::vector<std::mutex>& mutexes) {

    int rows = features.rows();
    int part = (rows / threads) + 1;
    int partStart = threadId * part;
    int partEnd = std::min((threadId + 1) * part, rows);

    for (int r = partStart; r < partEnd; ++r) {
        if (threadId == 0) printProgress(r, partEnd);
        int rFeaturesSize = features.size(r);
        int rLabelsSize = labels.size(r);
        auto rFeatures = features[r];
        auto rLabels = labels[r];
        for (int i = 0; i < rFeaturesSize; ++i) {
            for (int j = 0; j < rLabelsSize; ++j) {
                if (rFeatures[i].index == 1) continue; // Skip bias feature

                std::mutex& m = mutexes[rLabels[j] % mutexes.size()];
                m.lock();

                auto v = rFeatures[i].value;
                if (weightedFeatures) v /= rLabelsSize;
                tmpLabelsFeatures[rLabels[j]][rFeatures[i].index] += v;

                m.unlock();
            }
        }
    }
}

void computeLabelsFeaturesMatrix(SRMatrix<Feature>& labelsFeatures, const SRMatrix<Label>& labels,
                                 const SRMatrix<Feature>& features, int threads, bool norm, bool weightedFeatures) {

    std::vector<UnorderedMap<int, double>> tmpLabelsFeatures(labels.cols());
    assert(features.rows() == labels.rows());

    std::cerr << "Computing labels' features matrix in " << threads << " threads ...\n";

    std::vector<std::mutex> mutexes(1031); // First prime number larger then 1024
    ThreadSet tSet;
    for (int t = 0; t < threads; ++t)
        tSet.add(computeLabelsFeaturesMatrixThread, std::ref(tmpLabelsFeatures), std::ref(labels), std::ref(features),
                 weightedFeatures, t, threads, std::ref(mutexes));
    tSet.joinAll();

    std::vector<Prediction> labelsProb;
    if (!norm) labelsProb = computeLabelsPriors(labels);

    for (int l = 0; l < labels.cols(); ++l) {
        std::vector<Feature> labelFeatures;
        labelFeatures.reserve(tmpLabelsFeatures.size());
        for (const auto& f : tmpLabelsFeatures[l]) labelFeatures.push_back({f.first, f.second});
        std::sort(labelFeatures.begin(), labelFeatures.end());

        if (norm) unitNorm(labelFeatures);
        else divVector(labelFeatures, labelsProb[l].value * labels.rows());

        labelsFeatures.appendRow(labelFeatures);
    }
}

// Splits string
std::vector<std::string> split(std::string text, char d) {
    std::vector<std::string> tokens;
    const char* str = text.c_str();
    std::string strD = std::string("") + d;

    do {
        const char* begin = str;
        while (*str != d && *str) ++str;
        std::string token = std::string(begin, str);
        if (token.length() && token != strD) tokens.emplace_back(begin, str);
    } while (0 != *str++);

    return tokens;
}

std::string toLower(std::string text) {
    std::string lower = text;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
    return lower;
}

std::string formatMem(size_t mem){
    // kilo, mega, giga, tera, peta, exa
    char units[7] = {' ', 'K', 'M', 'G', 'T', 'P', 'E'};
    double fMem = mem;
    int i = 0;
    while(fMem > 1024){
        fMem /= 1024;
        ++i;
    }
    mem = std::ceil(fMem);

    return "~" + std::to_string(mem) + units[i];
}

// Files utils
void FileHelper::saveToFile(std::string outfile) {
    std::ofstream out(outfile);
    save(out);
    out.close();
}

void FileHelper::loadFromFile(std::string infile) {
    checkFileName(infile);
    std::ifstream in(infile);
    load(in);
    in.close();
}

// Joins two paths
std::string joinPath(const std::string& path1, const std::string& path2) {
    char sep = '/';

    std::string joined = path1;
    if (path1[path1.size() - 1] != sep) joined += sep;
    if (path2[0] == sep)
        joined += path2.substr(1);
    else
        joined += path2;

    return (joined);
}

// Checks filename
void checkFileName(const std::string& filename, bool read) {
    bool valid;
    if (read) {
        std::ifstream in(filename);
        valid = in.good();
    } else {
        std::ofstream out(filename);
        valid = out.good();
    }
    if (!valid) throw std::invalid_argument("Invalid filename: \"" + filename + "\"!");
}

// Checks dirname
void checkDirName(const std::string& dirname) {
    std::string tmpFile = joinPath(dirname, ".checkTmp");
    std::ofstream out(tmpFile);
    if (!out.good()) throw std::invalid_argument("Invalid dirname: \"" + dirname + "\"!");
    std::remove(tmpFile.c_str());
}

// TODO improve this
// Run shell CMD
void shellCmd(const std::string& cmd) {
    const int cmdErr = std::system(cmd.c_str());
    if (-1 == cmdErr) { exit(1); }
}

// Create directory
void makeDir(const std::string& dirname) {
    std::string mkdirCmd = "mkdir -p " + dirname;
    shellCmd(mkdirCmd);
}

// Remove directory of file
void remove(const std::string& path) {
    std::string rmCmd = "rm -rf " + path;
    shellCmd(rmCmd);
}
