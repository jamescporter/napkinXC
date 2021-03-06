/**
 * Copyright (c) 2018-2020 by Marek Wydmuch
 * All rights reserved.
 */

#include <cassert>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <iostream>

#include "args.h"
#include "resources.h"
#include "version.h"

Args::Args() {
    command = "";
    seed = time(nullptr);
    rngSeeder.seed(seed);

    // Input/output options
    input = "";
    output = "";
    dataFormatName = "libsvm";
    dataFormatType = libsvm;
    modelName = "plt";
    modelType = plt;
    header = true;
    hash = 0;
    bias = true;
    biasValue = 1.0;
    norm = true;
    featuresThreshold = 0.0;

    // Training options
    threads = getCpuCount();
    memLimit = getSystemMemory();
    eps = 0.1;
    cost = 16.0;
    maxIter = 100;
    autoCLin = false;
    autoCLog = false;

    solverType = L2R_LR_DUAL;
    solverName = "L2R_LR_DUAL";
    inbalanceLabelsWeighting = false;
    pickOneLabelWeighting = false;
    optimizerName = "liblinear";
    optimizerType = liblinear;
    weightsThreshold = 0.1;

    // Ensemble options
    ensemble = 0;
    onTheTrotPrediction = false;

    // For online training
    eta = 1.0;
    epochs = 1;
    tmax = -1;
    l2Penalty = 0;
    fobosPenalty = 0.00001;
    adagradEps = 0.001;
    dims = 100;

    // Tree options
    treeStructure = "";
    arity = 2;
    treeType = hierarchicalKMeans;
    treeTypeName = "hierarchicalKMeans";
    maxLeaves = 100;

    // K-Means tree options
    kMeansEps = 0.0001;
    kMeansBalanced = true;
    kMeansWeightedFeatures = false;

    // Online PLT options
    onlineTreeAlpha = 0.5;

    // Prediction options
    topK = 5;
    threshold = 0.0;
    thresholds = "";
    ensMissingScores = true;

    // Mips options
    mipsDense = false;
    hnswM = 20;
    hnswEfConstruction = 100;
    hnswEfSearch = 100;

    // Set utility options
    ubopMipsK = 0.05;

    setUtilityType = uP;
    alpha = 0.0;
    beta = 1.0;
    delta = 2.2;
    gamma = 1.2;


    // Measures for test command
    measures = "p@1,r@1,c@1,p@3,r@3,c@3,p@5,r@5,c@5";

    // Args for OFO command
    ofoType = micro;
    ofoTypeName = "micro";
    ofoTopLabels = 1000;
    ofoA = 10;
    ofoB = 20;

    // Args for testPredictionTime command
    batchSizes = "100,1000,10000";
    batches = 10;
}

// Parse args
void Args::parseArgs(const std::vector<std::string>& args) {
    command = args[1];

    if (command == "-h" || command == "--help" || command == "help") printHelp();
    if (command == "-v" || command == "--version" || command == "version") {

    }

    if (command != "train" && command != "test" && command != "predict" && command != "ofo" && command != "testPredictionTime") {
        std::cerr << "Unknown command type: " << command << "!\n";
        printHelp();
    }

    for (int ai = 2; ai < args.size(); ai += 2) {
        if (args[ai][0] != '-') {
            std::cerr << "Provided argument without a dash: " << args[ai] << "!\n";
            printHelp();
        }

        try {
            if (args[ai] == "--seed") {
                seed = std::stoi(args.at(ai + 1));
                rngSeeder.seed(seed);
            }

            // Input/output options
            else if (args[ai] == "-i" || args[ai] == "--input")
                input = std::string(args.at(ai + 1));
            else if (args[ai] == "-o" || args[ai] == "--output")
                output = std::string(args.at(ai + 1));
            else if (args[ai] == "-d" || args[ai] == "--dataFormat") {
                dataFormatName = args.at(ai + 1);
                if (args.at(ai + 1) == "libsvm")
                    dataFormatType = libsvm;
                else if (args.at(ai + 1) == "vw" || args.at(ai + 1) == "vowpalwabbit")
                    dataFormatType = vw;
                else {
                    std::cerr << "Unknown date format type: " << args.at(ai + 1) << "!\n";
                    printHelp();
                }
            } else if (args[ai] == "--ensemble")
                ensemble = std::stoi(args.at(ai + 1));
            else if (args[ai] == "--onTheTrotPrediction")
                onTheTrotPrediction = std::stoi(args.at(ai + 1));
            else if (args[ai] == "-m" || args[ai] == "--model") {
                modelName = args.at(ai + 1);
                if (args.at(ai + 1) == "br")
                    modelType = br;
                else if (args.at(ai + 1) == "ovr")
                    modelType = ovr;
                else if (args.at(ai + 1) == "hsm")
                    modelType = hsm;
                else if (args.at(ai + 1) == "plt")
                    modelType = plt;
                else if (args.at(ai + 1) == "ubop")
                    modelType = ubop;
                else if (args.at(ai + 1) == "ubopHsm")
                    modelType = ubopHsm;
                else if (args.at(ai + 1) == "oplt")
                    modelType = oplt;
                else if (args.at(ai + 1) == "extremeText")
                    modelType = extremeText;
// Mips extension models
#ifdef MIPS_EXT
                else if (args.at(ai + 1) == "brMips")
                    modelType = brMips;
                else if (args.at(ai + 1) == "ubopMips")
                    modelType = ubopMips;
#else
                    else if (args.at(ai + 1) == "brMips" || args.at(ai + 1) == "ubopMips") {
                        std::cerr << args.at(ai + 1) << " model requires MIPS extension";
                        exit(EXIT_FAILURE);
                    }
#endif
                else {
                    std::cerr << "Unknown model type: " << args.at(ai + 1) << "!\n";
                    printHelp();
                }
            } else if (args[ai] == "--mipsDense")
                mipsDense = std::stoi(args.at(ai + 1)) != 0;
            else if (args[ai] == "--hnswM")
                hnswM = std::stoi(args.at(ai + 1));
            else if (args[ai] == "--hnswEfConstruction")
                hnswEfConstruction = std::stoi(args.at(ai + 1));
            else if (args[ai] == "--hnswEfSearch")
                hnswEfSearch = std::stoi(args.at(ai + 1));
            else if (args[ai] == "--ubopMipsK")
                ubopMipsK = std::stof(args.at(ai + 1));
            else if (args[ai] == "--setUtility") {
                setUtilityName = args.at(ai + 1);
                if (args.at(ai + 1) == "uP")
                    setUtilityType = uP;
                else if (args.at(ai + 1) == "uR")
                    setUtilityType = uP;
                else if (args.at(ai + 1) == "uF1")
                    setUtilityType = uF1;
                else if (args.at(ai + 1) == "uFBeta")
                    setUtilityType = uFBeta;
                else if (args.at(ai + 1) == "uExp")
                    setUtilityType = uExp;
                else if (args.at(ai + 1) == "uLog")
                    setUtilityType = uLog;
                else if (args.at(ai + 1) == "uDeltaGamma")
                    setUtilityType = uDeltaGamma;
                else if (args.at(ai + 1) == "uAlpha")
                    setUtilityType = uAlpha;
                else if (args.at(ai + 1) == "uAlphaBeta")
                    setUtilityType = uAlphaBeta;
                else {
                    std::cerr << "Unknown set utility type: " << args.at(ai + 1) << "!\n";
                    printHelp();
                }
            } else if (args[ai] == "--alpha")
                alpha = std::stof(args.at(ai + 1));
            else if (args[ai] == "--beta")
                beta = std::stof(args.at(ai + 1));
            else if (args[ai] == "--delta")
                delta = std::stof(args.at(ai + 1));
            else if (args[ai] == "--gamma")
                gamma = std::stof(args.at(ai + 1));

            else if (args[ai] == "--header")
                header = std::stoi(args.at(ai + 1)) != 0;
            else if (args[ai] == "--bias")
                bias = std::stoi(args.at(ai + 1)) != 0;
            else if (args[ai] == "--norm")
                norm = std::stoi(args.at(ai + 1)) != 0;
            else if (args[ai] == "--hash")
                hash = std::stoi(args.at(ai + 1));
            else if (args[ai] == "--featuresThreshold")
                featuresThreshold = std::stof(args.at(ai + 1));
            else if (args[ai] == "--weightsThreshold")
                weightsThreshold = std::stof(args.at(ai + 1));

            // Training options
            else if (args[ai] == "-t" || args[ai] == "--threads") {
                threads = std::stoi(args.at(ai + 1));
                if (threads == 0)
                    threads = getCpuCount();
                else if (threads == -1)
                    threads = getCpuCount() - 1;
            } else if (args[ai] == "--memLimit") {
                memLimit = static_cast<unsigned long long>(std::stof(args.at(ai + 1)) * 1024 * 1024 * 1024);
                if (memLimit == 0) memLimit = getSystemMemory();
            } else if (args[ai] == "-e" || args[ai] == "--eps")
                eps = std::stof(args.at(ai + 1));
            else if (args[ai] == "-c" || args[ai] == "-C" || args[ai] == "--cost")
                cost = std::stof(args.at(ai + 1));
            else if (args[ai] == "--maxIter")
                maxIter = std::stoi(args.at(ai + 1));
            else if (args[ai] == "--inbalanceLabelsWeighting")
                inbalanceLabelsWeighting = std::stoi(args.at(ai + 1)) != 0;
            else if (args[ai] == "--pickOneLabelWeighting")
                pickOneLabelWeighting = std::stoi(args.at(ai + 1)) != 0;
            else if (args[ai] == "--solver") {
                solverName = args.at(ai + 1);
                if (args.at(ai + 1) == "L2R_LR_DUAL")
                    solverType = L2R_LR_DUAL;
                else if (args.at(ai + 1) == "L2R_LR")
                    solverType = L2R_LR;
                else if (args.at(ai + 1) == "L1R_LR")
                    solverType = L1R_LR;
                else if (args.at(ai + 1) == "L2R_L2LOSS_SVC_DUAL")
                    solverType = L2R_L2LOSS_SVC_DUAL;
                else if (args.at(ai + 1) == "L2R_L2LOSS_SVC")
                    solverType = L2R_L2LOSS_SVC;
                else if (args.at(ai + 1) == "L2R_L1LOSS_SVC_DUAL")
                    solverType = L2R_L1LOSS_SVC_DUAL;
                else if (args.at(ai + 1) == "L1R_L2LOSS_SVC")
                    solverType = L1R_L2LOSS_SVC;
                else {
                    std::cerr << "Unknown solver type: " << args.at(ai + 1) << "!\n";
                    printHelp();
                }
            } else if (args[ai] == "--optimizer") {
                optimizerName = args.at(ai + 1);
                if (args.at(ai + 1) == "liblinear")
                    optimizerType = liblinear;
                else if (args.at(ai + 1) == "sgd")
                    optimizerType = sgd;
                else if (args.at(ai + 1) == "adagrad")
                    optimizerType = adagrad;
                else if (args.at(ai + 1) == "fobos")
                    optimizerType = fobos;
                else {
                    std::cerr << "Unknown optimizer type: " << args.at(ai + 1) << "!\n";
                    printHelp();
                }
            } else if (args[ai] == "-l" || args[ai] == "--lr" || args[ai] == "--eta")
                eta = std::stof(args.at(ai + 1));
            else if (args[ai] == "--epochs")
                epochs = std::stoi(args.at(ai + 1));
            else if (args[ai] == "--tmax")
                tmax = std::stoi(args.at(ai + 1));
            else if (args[ai] == "--adagradEps")
                adagradEps = std::stof(args.at(ai + 1));
            else if (args[ai] == "--fobosPenalty")
                fobosPenalty = std::stof(args.at(ai + 1));
            else if (args[ai] == "--l2Penalty")
                l2Penalty = std::stof(args.at(ai + 1));
            else if (args[ai] == "--dims")
                dims = std::stoi(args.at(ai + 1));

            // Tree options
            else if (args[ai] == "-a" || args[ai] == "--arity")
                arity = std::stoi(args.at(ai + 1));
            else if (args[ai] == "--maxLeaves")
                maxLeaves = std::stoi(args.at(ai + 1));
            else if (args[ai] == "--kMeansEps")
                kMeansEps = std::stof(args.at(ai + 1));
            else if (args[ai] == "--kMeansBalanced")
                kMeansBalanced = std::stoi(args.at(ai + 1)) != 0;
            else if (args[ai] == "--kMeansWeightedFeatures")
                kMeansWeightedFeatures = std::stoi(args.at(ai + 1)) != 0;
            else if (args[ai] == "--treeStructure") {
                treeStructure = std::string(args.at(ai + 1));
                treeType = custom;
            } else if (args[ai] == "--treeType") {
                treeTypeName = args.at(ai + 1);
                if (args.at(ai + 1) == "completeInOrder")
                    treeType = completeInOrder;
                else if (args.at(ai + 1) == "completeRandom")
                    treeType = completeRandom;
                else if (args.at(ai + 1) == "balancedInOrder")
                    treeType = balancedInOrder;
                else if (args.at(ai + 1) == "balancedRandom")
                    treeType = balancedRandom;
                else if (args.at(ai + 1) == "hierarchicalKMeans")
                    treeType = hierarchicalKMeans;
                else if (args.at(ai + 1) == "huffman")
                    treeType = huffman;
                else if (args.at(ai + 1) == "onlineKAryComplete")
                    treeType = onlineKAryComplete;
                else if (args.at(ai + 1) == "onlineKAryRandom")
                    treeType = onlineKAryRandom;
                else if (args.at(ai + 1) == "onlineRandom")
                    treeType = onlineRandom;
                else if (args.at(ai + 1) == "onlineBestScore")
                    treeType = onlineBestScore;

                else {
                    std::cerr << "Unknown tree type: " << args.at(ai + 1) << "!\n";
                    printHelp();
                }
            } else if (args[ai] == "--onlineTreeAlpha")
                onlineTreeAlpha = std::stof(args.at(ai + 1));

            // OFO options
            else if (args[ai] == "--ofoType") {
                ofoTypeName = args.at(ai + 1);
                if (args.at(ai + 1) == "micro")
                    ofoType = micro;
                else if (args.at(ai + 1) == "macro")
                    ofoType = macro;
                else if (args.at(ai + 1) == "mixed")
                    ofoType = mixed;
                else {
                    std::cerr << "Unknown ofo type: " << args.at(ai + 1) << "!\n";
                    printHelp();
                }
            } else if (args[ai] == "--ofoTopLabels")
                ofoTopLabels = std::stoi(args.at(ai + 1));
            else if (args[ai] == "--ofoA")
                ofoA = std::stoi(args.at(ai + 1));
            else if (args[ai] == "--ofoB")
                ofoB = std::stoi(args.at(ai + 1));

            // Prediction/test options
            else if (args[ai] == "--topK")
                topK = std::stoi(args.at(ai + 1));
            else if (args[ai] == "--threshold")
                threshold = std::stof(args.at(ai + 1));
            else if (args[ai] == "--thresholds")
                thresholds = std::string(args.at(ai + 1));
            else if (args[ai] == "--ensMissingScores")
                ensMissingScores = std::stoi(args.at(ai + 1)) != 0;

            else if (args[ai] == "--batchSizes")
                batchSizes = args.at(ai + 1);
            else if (args[ai] == "--batches")
                batches = std::stoi(args.at(ai + 1));

            else if (args[ai] == "--measures")
                measures = std::string(args.at(ai + 1));
            else if (args[ai] == "--autoCLin")
                autoCLin = std::stoi(args.at(ai + 1)) != 0;
            else if (args[ai] == "--autoCLog")
                autoCLog = std::stoi(args.at(ai + 1)) != 0;
            else {
                std::cerr << "Unknown argument: " << args[ai] << std::endl;
                printHelp();
            }

        } catch (std::out_of_range) {
            std::cerr << args[ai] << " is missing an argument!\n";
            printHelp();
        }
    }

    if (input.empty() || output.empty()) {
        std::cerr << "Empty input or model path!\n";
        printHelp();
    }

    // Change default values for specific cases + parameters warnings
    if (modelType == oplt && optimizerType == liblinear) {
        if (count(args.begin(), args.end(), "optimizer"))
            std::cerr << "Online PLT does not support " << optimizerName << " optimizer! Changing to AdaGrad.\n";
        optimizerType = adagrad;
        optimizerName = "adagrad";
    }

    if (modelType == oplt && (treeType == hierarchicalKMeans || treeType == huffman)) {
        if (count(args.begin(), args.end(), "treeType"))
            std::cerr << "Online PLT does not support " << treeTypeName
                      << " tree type! Changing to complete in order tree.\n";
        treeType = onlineBestScore;
        treeTypeName = "onlineBestScore";
    }

    // If only threshold used set topK to 0, otherwise display warning
    if (threshold > 0) {
        if (count(args.begin(), args.end(), "topK"))
            std::cerr << "Warning: Top K and threshold prediction are used at the same time!\n";
        else
            topK = 0;
    }

    // Warnings about arguments overrides while testing / predicting
}

void Args::printArgs() {
    std::cerr << "napkinXC " << VERSION << " - " << command
              << "\n  Input: " << input << "\n    Data format: " << dataFormatName
              << "\n    Header: " << header << ", bias: " << bias << ", norm: " << norm << ", hash size: " << hash << ", features threshold: " << featuresThreshold
              << "\n  Model: " << output << "\n    Type: " << modelName;

    if (ensemble > 1) std::cerr << ", ensemble: " << ensemble;

    if (command == "train") {
        std::cerr << "\n  Base models optimizer: " << optimizerName;
        if (optimizerType == liblinear)
            std::cerr << "\n    Solver: " << solverName << ", eps: " << eps << ", cost: " << cost << ", max iter: " << maxIter;
        else
            std::cerr << "\n    Eta: " << eta << ", epochs: " << epochs;
        if (optimizerType == adagrad) std::cerr << ", AdaGrad eps " << adagradEps;
        if (optimizerType == fobos) std::cerr << ", Fobos penalty: " << fobosPenalty;
        std::cerr << ", weights threshold: " << weightsThreshold;

        if (modelType == plt || modelType == hsm || modelType == oplt || modelType == ubopHsm) {
            if (treeStructure.empty()) {
                std::cerr << "\n  Tree type: " << treeTypeName << ", arity: " << arity;
                if (treeType == hierarchicalKMeans)
                    std::cerr << ", k-means eps: " << kMeansEps << ", balanced: " << kMeansBalanced
                              << ", weighted features: " << kMeansWeightedFeatures;
                if (treeType == hierarchicalKMeans || treeType == balancedInOrder || treeType == balancedRandom)
                    std::cerr << ", max leaves: " << maxLeaves;
            } else {
                std::cerr << "\n    Tree: " << treeStructure;
            }
        }
    }

    if (command == "test") {
        if(thresholds.empty()) std::cerr << "\n  Top k: " << topK << ", threshold: " << threshold;
        else std::cerr << "\n  Thresholds: " << thresholds;
        if (modelType == ubopMips || modelType == brMips) {
            std::cerr << "\n  HNSW: M: " << hnswM << ", efConst.: " << hnswEfConstruction << ", efSearch: " << hnswEfSearch;
            if(modelType == ubopMips) std::cerr << ", k: " << ubopMipsK;
        }
        if (modelType == ubop || modelType == ubopHsm || modelType == ubopMips) {
            std::cerr << "\n  Set utility: " << setUtilityName;
            if (setUtilityType == uAlpha || setUtilityType == uAlphaBeta) std::cerr << ", alpha: " << alpha;
            if (setUtilityType == uAlphaBeta) std::cerr << ", beta: " << beta;
            if (setUtilityType == uDeltaGamma) std::cerr << ", delta: " << delta << ", gamma: " << gamma;
        }
    }

    if (command == "ofo")
        std::cerr << "\n  Epochs: " << epochs << ", a: " << ofoA << ", b: " << ofoB;

    std::cerr << "\n  Threads: " << threads << ", memory limit: " << formatMem(memLimit)
              << "\n  Seed: " << seed << std::endl;
}

void Args::printHelp() {
    std::cerr << R"HELP(Usage: nxc [command] [args ...]

Commands:
    train
    test
    predict
    ofo
    testPredictionTime

Args:
    General:
    -i, --input         Input dataset
    -o, --output        Output (model) dir
    -m, --model         Model type (default = plt):
                        Models: ovr, br, hsm, plt, oplt, ubop, ubopHsm, brMips, ubopMips
    --ensemble          Ensemble of models (default = 0)
    -d, --dataFormat    Type of data format (default = libsvm):
                        Supported data formats: libsvm
    -t, --threads       Number of threads used for training and testing (default = 0)
                        Note: -1 to use system #cpus - 1, 0 to use system #cpus
    --memLimit          Amount of memory in GB used for training OVR and BR models (default = 0)
                        Note: 0 to use system memory
    --header            Input contains header (default = 1)
                        Header format for libsvm: #lines #features #labels
    --hash              Size of features space (default = 0)
                        Note: 0 to disable hashing
    --featuresThreshold Prune features belowe given threshold (default = 0.0)
    --seed              Seed

    Base classifiers:
    --optimizer         Use LibLiner or online optimizers (default = libliner)
                        Optimizers: liblinear, sgd, adagrad, fobos
    --bias              Add bias term (default = 1)
    --weightsThreshold  Prune weights below given threshold (default = 0.1)
    --inbalanceLabelsWeighting     Increase the weight of minority labels in base classifiers (default = 0)

    LibLinear:
    -s, --solver        LibLinear solver (default = L2R_LR_DUAL)
                        Supported solvers: L2R_LR_DUAL, L2R_LR, L1R_LR,
                                           L2R_L2LOSS_SVC_DUAL, L2R_L2LOSS_SVC, L2R_L1LOSS_SVC_DUAL, L1R_L2LOSS_SVC
                        See: https://github.com/cjlin1/liblinear
    -c, -C, --cost      Inverse of regularization strength. Must be a positive float.
                        Smaller values specify stronger regularization. (default = 10.0)
                        Note: -1 to automatically find best value for each node.
    -e, --eps           Stopping criteria (default = 0.1)
                        See: https://github.com/cjlin1/liblinear

    SGD/AdaGrad/Fobos:
    -l, --lr, --eta     Step size (learning rate) of SGD/AdaGrad/Fobos (default = 1.0)
    --epochs            Number of epochs of SGD/AdaGrad/Fobos (default = 5)
    --adagradEps        AdaGrad epsilon (default = 0.001)
    --fobosPenalty      Regularization strength of Fobos algorithm (default = 0.00001)

    Tree:
    -a, --arity         Arity of a tree (default = 2)
    --maxLeaves         Maximum number of leaves (labels) in one internal node.
                        Supported by k-means and balanced trees. (default = 100)
    --tree              File with tree structure
    --treeType          Type of a tree to build if file with structure is not provided
                        Tree types: hierarchicalKMeans, huffman, completeInOrder, completeRandom,
                                    balancedInOrder, balancedRandom, onlineComplete, onlineBalanced,
                                    onlineRandom

    K-means tree:
    --kMeansEps         Stopping criteria for K-Means clustering (default = 0.001)
    --kMeansBalanced    Use balanced K-Means clustering (default = 1)

    Prediction:
    --topK              Predict top k elements (default = 5)
    --threshold         Probability threshold (default = 0)
    --setUtility        Type of set-utility function for prediction using ubop, ubopHsm, ubopMips models.
                        Set-utility functions: uP, uF1, uAlpha, uAlphaBeta, uDeltaGamma
                        See: https://arxiv.org/abs/1906.08129

    Set-Utility:
    --alpha
    --beta
    --delta
    --gamma

    Test:
    --measures          Evaluate test using set of measures (default = "p@1,r@1,c@1,p@3,r@3,c@3,p@5,r@5,c@5")
                        Measures: acc (accuracy), p (precision), r (recall), c (coverage),
                                  p@k (precision at k), r@k (recall at k), c@k (coverage at k), s (prediction size)

    )HELP";
    exit(EXIT_FAILURE);
}

void Args::save(std::ostream& out) {
    out.write((char*)&bias, sizeof(bias));
    out.write((char*)&norm, sizeof(norm));
    out.write((char*)&hash, sizeof(hash));
    out.write((char*)&modelType, sizeof(modelType));
    out.write((char*)&dataFormatType, sizeof(dataFormatType));
    // out.write((char*) &ensemble, sizeof(ensemble));

    saveVar(out, modelName);
    saveVar(out, dataFormatName);
}

void Args::load(std::istream& in) {
    in.read((char*)&bias, sizeof(bias));
    in.read((char*)&norm, sizeof(norm));
    in.read((char*)&hash, sizeof(hash));
    in.read((char*)&modelType, sizeof(modelType));
    in.read((char*)&dataFormatType, sizeof(dataFormatType));
    // in.read((char*) &ensemble, sizeof(ensemble));

    loadVar(in, modelName);
    loadVar(in, dataFormatName);
}
