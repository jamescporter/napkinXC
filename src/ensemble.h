/**
 * Copyright (c) 2019 by Marek Wydmuch
 * All rights reserved.
 */

#pragma once

#include <unordered_map>
#include <unordered_set>

#include "model.h"

struct EnsemblePrediction {
    int label;
    double value;
    std::vector<int> members;

    bool operator<(const EnsemblePrediction& r) const { return value < r.value; }
};


template <typename T> class Ensemble : public Model {
public:
    Ensemble();
    ~Ensemble() override;

    void train(SRMatrix<Label>& labels, SRMatrix<Feature>& features, Args& args, std::string output) override;
    void predict(std::vector<Prediction>& prediction, Feature* features, Args& args) override;
    double predictForLabel(Label label, Feature* features, Args& args) override;
    std::vector<std::vector<Prediction>> predictBatch(SRMatrix<Feature>& features, Args& args) override;

    void predictWithThresholds(std::vector<Prediction>& prediction, Feature* features, Args& args) override;

    void load(Args& args, std::string infile) override;

    void printInfo() override;

protected:
    std::vector<T*> members;
    T* loadMember(Args& args, const std::string& infile, int memberNo);
    void accumulatePrediction(std::unordered_map<int, EnsemblePrediction>& ensemblePredictions,
                              std::vector<Prediction>& prediction, int memberNo);
};


template <typename T> Ensemble<T>::Ensemble() {}

template <typename T> Ensemble<T>::~Ensemble() {
    for (auto& m : members) delete m;
}

template <typename T>
void Ensemble<T>::train(SRMatrix<Label>& labels, SRMatrix<Feature>& features, Args& args, std::string output) {
    std::cerr << "Training ensemble of " << args.ensemble << " models ...\n";

    for (int i = 0; i < args.ensemble; ++i) {
        std::string memberDir = joinPath(output, "member_" + std::to_string(i));
        makeDir(memberDir);
        T* member = new T();
        member->train(labels, features, args, memberDir);
        delete member;
    }
}

template <typename T>
void Ensemble<T>::accumulatePrediction(std::unordered_map<int, EnsemblePrediction>& ensemblePredictions,
                                       std::vector<Prediction>& prediction, int memberNo) {

    for (auto& mP : prediction) {
        auto ensP = ensemblePredictions.find(mP.label);
        if (ensP != ensemblePredictions.end()) {
            ensP->second.value += mP.value;
            ensP->second.members.push_back(memberNo);
        } else
            ensemblePredictions.insert({mP.label, {mP.label, mP.value, {memberNo}}});
    }
}

template <typename T> void Ensemble<T>::predict(std::vector<Prediction>& prediction, Feature* features, Args& args) {

    std::unordered_map<int, EnsemblePrediction> ensemblePredictions;
    for (size_t i = 0; i < members.size(); ++i) {
        prediction.clear();
        members[i]->predict(prediction, features, args);
        accumulatePrediction(ensemblePredictions, prediction, i);
    }

    prediction.clear();
    for (auto& p : ensemblePredictions) {
        if(args.ensMissingScores) {
            for (size_t i = 0; i < members.size(); ++i) {
                if (!std::count(p.second.members.begin(), p.second.members.end(), i))
                    p.second.value += members[i]->predictForLabel(p.second.label, features, args);
            }
        }
        prediction.push_back({p.second.label, p.second.value / members.size()});
    }

    sort(prediction.rbegin(), prediction.rend());
    if (args.topK > 0) prediction.resize(args.topK);
}

template <typename T> double Ensemble<T>::predictForLabel(Label label, Feature* features, Args& args) {
    double value = 0;
    for (auto& m : members) value += m->predictForLabel(label, features, args);
    return value / members.size();
}

template <typename T>
std::vector<std::vector<Prediction>> Ensemble<T>::predictBatch(SRMatrix<Feature>& features, Args& args) {
    if (!args.onTheTrotPrediction) return Model::predictBatch(features, args);

    int rows = features.rows();
    std::vector<std::unordered_map<int, EnsemblePrediction>> ensemblePredictions(rows);

    // Get top predictions for members
    for (int memberNo = 0; memberNo < args.ensemble; ++memberNo) {
        T* member = loadMember(args, args.output, memberNo);

        std::vector<std::vector<Prediction>> memberPredictions = member->predictBatch(features, args);
        for (int i = 0; i < rows; ++i) accumulatePrediction(ensemblePredictions[i], memberPredictions[i], memberNo);

        delete member;
    }

    // Predict missing predictions for specific labels
    if(args.ensMissingScores) {
        for (int memberNo = 0; memberNo < args.ensemble; ++memberNo) {
            T *member = loadMember(args, args.output, memberNo);

            for (int i = 0; i < rows; ++i) {
                printProgress(i, rows);
                for (auto &p : ensemblePredictions[i]) {
                    if (!std::count(p.second.members.begin(), p.second.members.end(), memberNo))
                        p.second.value += member->predictForLabel(p.second.label, features[i], args);
                }
            }

            delete member;
        }
    }

    // Create final predictions
    std::vector<std::vector<Prediction>> predictions(rows);
    for (int i = 0; i < rows; ++i) {
        for (auto& p : ensemblePredictions[i])
            predictions[i].push_back({p.second.label, p.second.value / args.ensemble});
        sort(predictions[i].rbegin(), predictions[i].rend());
        if (args.topK > 0) predictions[i].resize(args.topK);
    }

    return predictions;
}

template <typename T> T* Ensemble<T>::loadMember(Args& args, const std::string& infile, int memberNo) {
    std::cerr << "  Loading ensemble member number " << memberNo << " ...\n";
    assert(memberNo < args.ensemble);
    T* member = new T();
    member->load(args, joinPath(infile, "member_" + std::to_string(memberNo)));
    return member;
}

template <typename T> void Ensemble<T>::load(Args& args, std::string infile) {
    if (!args.onTheTrotPrediction) {
        std::cerr << "Loading ensemble of " << args.ensemble << " models ...\n";
        for (int i = 0; i < args.ensemble; ++i) members.push_back(loadMember(args, infile, i));
        m = members[0]->outputSize();
    } else {
        T* member = loadMember(args, infile, 0);
        m = member->outputSize();
        delete member;
    }
}

template <typename T> void Ensemble<T>::printInfo() {}


template <typename T> void Ensemble<T>::predictWithThresholds(std::vector<Prediction>& prediction, Feature* features, Args& args){
    std::cerr << "  Threshold prediction is not available for ensemble";
}