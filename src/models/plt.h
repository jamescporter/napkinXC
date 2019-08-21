/**
 * Copyright (c) 2018 by Marek Wydmuch, Kalina Jasińska, Robert Istvan Busa-Fekete
 * Copyright (c) 2019 by Marek Wydmuch
 * All rights reserved.
 */

#pragma once

#include "base.h"
#include "model.h"
#include "tree.h"


class PLT: public Model{
public:
    PLT();
    ~PLT() override;

    void train(SRMatrix<Label>& labels, SRMatrix<Feature>& features, Args &args, std::string output) override;
    void predict(std::vector<Prediction>& prediction, Feature* features, Args &args) override;
    double predict(Label label, Feature* features, Args &args) override;

    void load(Args &args, std::string infile) override;

    void printInfo() override;

protected:
    Tree* tree;
    std::vector<Base*> bases;

    void predictNext(std::priority_queue<TreeNodeValue>& nQueue, std::vector<Prediction>& prediction, Feature* features);

    int nCount; // Number of visited nodes (updated/evaluated classifiers) during training or prediction
    int rCount; // Data points count
};