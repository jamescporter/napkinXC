/**
 * Copyright (c) 2019 by Marek Wydmuch
 * All rights reserved.
 */

#pragma once

#include "base.h"
#include "model.h"


class BR : public Model {
public:
    BR();
    ~BR() override;

    void train(SRMatrix<Label>& labels, SRMatrix<Feature>& features, Args& args, std::string output) override;
    void predict(std::vector<Prediction>& prediction, Feature* features, Args& args) override;
    double predictForLabel(Label label, Feature* features, Args& args) override;

    void predictWithThresholds(std::vector<Prediction>& prediction, Feature* features, Args& args) override;

    void load(Args& args, std::string infile) override;

    void printInfo() override;

protected:
    std::vector<Base*> bases;

    virtual std::vector<Prediction> predictForAllLabels(Feature* features, Args& args);
    static size_t calculateNumberOfParts(SRMatrix<Label>& labels, SRMatrix<Feature>& features, Args& args);
};
