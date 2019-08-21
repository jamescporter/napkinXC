/**
 * Copyright (c) 2019 by Marek Wydmuch
 * All rights reserved.
 */

#pragma once

#include <string>
#include <fstream>
#include <vector>
#include <unordered_map>
#include <queue>
#include <tuple>
#include <algorithm>
#include <random>

#include "base.h"
#include "model.h"
#include "tree.h"


class HSM: public Model{
public:
    HSM();
    ~HSM() override;

    void train(SRMatrix<Label>& labels, SRMatrix<Feature>& features, Args &args, std::string output) override;
    void predict(std::vector<Prediction>& prediction, Feature* features, Args &args) override;
    double predict(Label label, Feature* features, Args &args) override;

    void load(Args &args, std::string infile) override;

    void printInfo() override;

protected:
    Tree* tree;
    std::vector<Base*> bases;

    void predictNext(std::priority_queue<TreeNodeValue>& nQueue, std::vector<Prediction>& prediction, Feature* features);

    int eCount; // Number of updated/evaluated classifiers during training or prediction
    int pLen; // Len of the path
    int rCount;
};