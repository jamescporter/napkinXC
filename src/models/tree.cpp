/**
 * Copyright (c) 2018 by Marek Wydmuch, Kalina Jasinska, Robert Istvan Busa-Fekete
 * Copyright (c) 2019 by Marek Wydmuch
 * All rights reserved.
 */

#include <cassert>
#include <sstream>
#include <unordered_set>
#include <algorithm>
#include <vector>
#include <list>
#include <chrono>
#include <random>
#include <cmath>
#include <climits>
#include <iomanip>

#include "tree.h"
#include "threads.h"

Tree::Tree(){}

Tree::~Tree() {
    for(size_t i = 0; i < nodes.size(); ++i)
        delete nodes[i];
}

void Tree::buildTreeStructure(SRMatrix<Label>& labels, SRMatrix<Feature>& features, Args& args){
    rng.seed(args.seed);


    // Load tree structure from file
    if (!args.treeStructure.empty())
        loadTreeStructure(args.treeStructure);
    // Create a tree structure
    else if (args.treeType == completeInOrder)
        buildCompleteTree(labels.cols(), false, args);
    else if (args.treeType == completeRandom)
        buildCompleteTree(labels.cols(), true, args);
    else if (args.treeType == balancedInOrder)
        buildBalancedTree(labels.cols(), false, args);
    else if (args.treeType == balancedRandom)
        buildBalancedTree(labels.cols(), true, args);
    else if (args.treeType == huffman)
        buildHuffmanTree(labels, args);
    else if (args.treeType == hierarchicalKMeans) {
        SRMatrix<Feature> labelsFeatures;
        computeLabelsFeaturesMatrix(labelsFeatures, labels, features, args.threads, args.norm, args.kMeansWeightedFeatures);
        //labelsFeatures.save(joinPath(args.model, "lf_mat.bin"));
        buildKMeansTree(labelsFeatures, args);
    }
    else {
        std::cerr << "Unknown tree type\n";
        exit(0);
    }

    // Save structure
    saveTreeStructure(joinPath(args.output, "tree.txt"));

    // Check tree
    assert(k == leaves.size());
    assert(t == nodes.size());
}

TreeNodePartition treeNodeKMeansThread(TreeNodePartition nPart, SRMatrix<Feature>& labelsFeatures, Args& args, int seed){
    kMeans(nPart.partition, labelsFeatures, args.arity, args.kMeansEps, args.kMeansBalanced, seed);
    return nPart;
}

void Tree::buildKMeansTree(SRMatrix<Feature>& labelsFeatures, Args& args){
    std::cerr << "Hierarchical K-Means clustering in " << args.threads << " threads ...\n";

    root = createTreeNode();
    k = labelsFeatures.rows();

    std::uniform_int_distribution<int> kMeansSeeder(0, INT_MAX);

    auto partition = new std::vector<Assignation>(k);
    for(int i = 0; i < k; ++i) (*partition)[i].index = i;

    if(args.threads > 1){
        // Run clustering in parallel
        ThreadPool tPool(args.threads);
        std::vector<std::future<TreeNodePartition>> results;

        TreeNodePartition rootPart = {root, partition};
        results.emplace_back(tPool.enqueue(treeNodeKMeansThread, rootPart, std::ref(labelsFeatures),
                                           std::ref(args), kMeansSeeder(rng)));

        for(int r = 0; r < results.size(); ++r) {
            // Enqueuing new clustering tasks in the main thread ensures determinism
            TreeNodePartition nPart = results[r].get();

            // This needs to be done this way in case of imbalanced K-Means
            auto partitions = new std::vector<Assignation>* [args.arity];
            for (int i = 0; i < args.arity; ++i) partitions[i] = new std::vector<Assignation>();
            for (auto a : *nPart.partition) partitions[a.value]->push_back({a.index, 0});

            for (int i = 0; i < args.arity; ++i) {
                if(partitions[i]->empty()) continue;
                else if(partitions[i]->size() == 1){
                    createTreeNode(nPart.node, partitions[i]->front().index);
                    delete partitions[i];
                    continue;
                }

                TreeNode *n = createTreeNode(nPart.node);

                if(partitions[i]->size() <= args.maxLeaves) {
                    for (const auto& a : *partitions[i]) createTreeNode(n, a.index);
                    delete partitions[i];
                } else {
                    TreeNodePartition childPart = {n, partitions[i]};
                    results.emplace_back(tPool.enqueue(treeNodeKMeansThread, childPart, std::ref(labelsFeatures),
                                                       std::ref(args), kMeansSeeder(rng)));
                }
            }

            delete nPart.partition;
        }
    } else {
        std::queue<TreeNodePartition> nQueue;
        nQueue.push({root, partition});

        while (!nQueue.empty()) {
            TreeNodePartition nPart = nQueue.front(); // Current node
            nQueue.pop();

            if (nPart.partition->size() > args.maxLeaves) {
                kMeans(nPart.partition, labelsFeatures, args.arity, args.kMeansEps, args.kMeansBalanced, kMeansSeeder(rng));
                auto partitions = new std::vector<Assignation>* [args.arity];
                for (int i = 0; i < args.arity; ++i) partitions[i] = new std::vector<Assignation>();
                for (auto a : *nPart.partition) partitions[a.value]->push_back({a.index, 0});

                // Create children
                for (int i = 0; i < args.arity; ++i) {
                    TreeNode *n = createTreeNode(nPart.node);
                    nQueue.push({n, partitions[i]});
                }
            } else
                for (const auto& a : *nPart.partition) createTreeNode(nPart.node, a.index);

            delete nPart.partition;
        }
    }

    t = nodes.size();
    assert(k == leaves.size());
    std::cerr << "  Nodes: " << nodes.size() << ", leaves: " << leaves.size() << "\n";
}

void Tree::buildHuffmanTree(SRMatrix<Label>& labels, Args &args){
    std::cout << "Building Huffman Tree ...\n";

    k = labels.cols();

    std::vector<Frequency> labelsFreq;
    computeLabelsFrequencies(labelsFreq, labels);

    std::priority_queue<TreeNodeFrequency> freqQueue;
    for(int i = 0; i < k; i++) {
        TreeNode* n = createTreeNode(nullptr, i);
        freqQueue.push({n, labelsFreq[i].value});
    }

    while(!freqQueue.empty()){
        std::vector<TreeNodeFrequency> toMerge;
        for(int a = 0; a < args.arity; ++a){
            toMerge.push_back(freqQueue.top());
            freqQueue.pop();
            if (freqQueue.empty()) break;
        }

        TreeNode* parent = createTreeNode();
        int aggregatedFreq = 0;
        for(TreeNodeFrequency& e : toMerge){
            e.node->parent = parent;
            parent->children.push_back(e.node);
            aggregatedFreq += e.frequency;
        }

        nodes.push_back(parent);

        if (freqQueue.empty()) root = parent;
        freqQueue.push({parent, aggregatedFreq});
    }

    t = nodes.size();  // size of the tree
    std::cout << "  Nodes: " << nodes.size() << ", leaves: " << leaves.size() << ", arity: " << args.arity << "\n";
}

void Tree::buildBalancedTree(int labelCount, bool randomizeOrder, Args &args) {
    std::cerr << "Building balanced Tree ...\n";

    root = createTreeNode();
    k = labelCount;

    auto partition = new std::vector<Assignation>(k);
    for(int i = 0; i < k; ++i) (*partition)[i].index = i;

    if (randomizeOrder) std::shuffle(partition->begin(), partition->end(), rng);

    std::queue<TreeNodePartition> nQueue;
    nQueue.push({root, partition});

    while (!nQueue.empty()) {
        TreeNodePartition nPart = nQueue.front(); // Current node
        nQueue.pop();
        if (nPart.partition->size() > args.maxLeaves) {
            auto partitions = new std::vector<Assignation>* [args.arity];
            for(int i = 0; i < args.arity; ++i) partitions[i] = new std::vector<Assignation>();

            int maxPartitionSize = nPart.partition->size() / args.arity;
            int maxWithOneMore = nPart.partition->size() % args.arity;
            int nextPartition = maxPartitionSize + (maxWithOneMore > 0 ? 1 : 0);
            int partitionNumber = 0;

            for (int i = 0; i < nPart.partition->size(); ++i) {
                if (i == nextPartition) {
                    ++partitionNumber;
                    --maxWithOneMore;
                    nextPartition += maxPartitionSize + (maxWithOneMore > 0 ? 1 : 0);
                    assert(partitionNumber < args.arity);
                }
                auto a = nPart.partition->at(i);
                partitions[partitionNumber]->push_back({a.index, 0});
            }
            assert(nextPartition == nPart.partition->size());

            // Create children
            for (int i = 0; i < args.arity; ++i) {
                TreeNode *n = createTreeNode(nPart.node);
                nQueue.push({n, partitions[i]});
            }
        } else
            for (const auto& a : *nPart.partition) createTreeNode(nPart.node, a.index);

        delete nPart.partition;
    }

    t = nodes.size();
    assert(k == leaves.size());
    std::cerr << "  Nodes: " << nodes.size() << ", leaves: " << leaves.size() << "\n";
}

void Tree::buildCompleteTree(int labelCount, bool randomizeOrder, Args &args) {
    std::cerr << "Building complete Tree ...\n";

    k = labelCount;
    t = static_cast<int>(ceil(static_cast<double>(args.arity * k - 1) / (args.arity - 1)));

    int ti = t - k;

    std::vector<int> labelsOrder;
    if (randomizeOrder){
        labelsOrder.resize(k);
        for (auto i = 0; i < k; ++i) labelsOrder[i] = i;
        std::shuffle(labelsOrder.begin(), labelsOrder.end(), rng);
    }

    root = createTreeNode();
    for(size_t i = 1; i < t; ++i){
        int label = -1;
        TreeNode *parent = nullptr;

        if(i >= ti){
            if(randomizeOrder) label = labelsOrder[i - ti];
            else label = i - ti;
        }

        parent = nodes[static_cast<int>(floor(static_cast<double>(i - 1) / args.arity))];
        createTreeNode(parent, label);
    }

    std::cerr << "  Nodes: " << nodes.size() << ", leaves: " << leaves.size() << ", arity: " << args.arity << "\n";
}


void Tree::loadTreeStructure(std::string file){
    std::cerr << "Loading Tree structure from: " << file << "...\n";

    std::ifstream in(file);
    in >> k >> t;

    if(k >= t) throw "Specified number of labels is higher then specified number of nodes!\n";

    root = createTreeNode();
    for (int i = 1; i < t; ++i) createTreeNode();

    std::cout << "  Header: nodes: " << t << ", leaves: " << k << "\n";

    std::string line;
    while(std::getline(in, line)){
        if(!line.length()) continue;

        int parent, child, label = -1;
        std::string sLabel;

        std::istringstream lineISS(line);
        lineISS >> parent >> child >> sLabel;
        //std::cout << parent << " " << child << " " << sLabel << " ";
        if(sLabel.size())
            label = std::stoi(sLabel);

        //std::cout << parent << " " << child << " " << label << "\n";

        if(child >= t) throw "Node index is higher then specified number of nodes!";
        if(parent >= t) throw "Parent index is higher then specified number of nodes!";
        if(label >= k) throw "Label index is higher then specified number of labels!";

        if(parent == -1){
            root = nodes[child];
            continue;
        }

        TreeNode *parentN = nodes[parent];
        TreeNode *childN = nodes[child];
        parentN->children.push_back(childN);
        childN->parent = parentN;

        if(label >= 0){
            assert(leaves.count(label) == 0);
            assert(label < k);
            childN->label = label;
            leaves[childN->label] = childN;
        }
    }
    in.close();

    // Additional validation of a tree
    for(const auto& n : nodes) {
        if(n->parent == nullptr && n != root) throw "A node without parent, that is not a tree root exists!";
        if(n->children.size() == 0 && n->label < 0) throw "An internal node without children exists!";
    }

    assert(nodes.size() == t);
    assert(leaves.size() == k);
    std::cout << "  Loaded: nodes: " << nodes.size() << ", leaves: " << leaves.size() << "\n";
}

void Tree::saveTreeStructure(std::string file) {
    std::cerr << "Saving Tree structure to: " << file << "...\n";

    std::ofstream out(file);
    out << k << " " << t << "\n";
    for (auto &n : nodes) {
        if (n->parent != nullptr) out << n->parent->index;
        //else out << -1

        out << " " << n->index << " ";

        if (n->label >= 0) out << n->label;
        //else out << -1;

        out << "\n";
    }
    out.close();
}

TreeNode* Tree::createTreeNode(TreeNode* parent, int label){
    TreeNode* n = new TreeNode();
    n->index = nodes.size();
    n->label = label;
    n->parent = parent;
    if(label >= 0) leaves[n->label] = n;
    if(parent != nullptr) parent->children.push_back(n);
    nodes.push_back(n);

    return n;
}

void Tree::save(std::ostream& out){
    std::cerr << "Saving tree ...\n";

    out.write((char*) &k, sizeof(k));

    t = nodes.size();
    out.write((char*) &t, sizeof(t));
    for(size_t i = 0; i < t; ++i) {
        TreeNode *n = nodes[i];
        out.write((char*) &n->index, sizeof(n->index));
        out.write((char*) &n->label, sizeof(n->label));
    }

    int rootN = root->index;
    out.write((char*) &rootN, sizeof(rootN));

    for(size_t i = 0; i < t; ++i) {
        TreeNode *n = nodes[i];

        int parentN;
        if(n->parent) parentN = n->parent->index;
        else parentN = -1;

        out.write((char*) &parentN, sizeof(parentN));
    }
}

void Tree::load(std::istream& in){
    std::cerr << "Loading tree ...\n";

    in.read((char*) &k, sizeof(k));
    in.read((char*) &t, sizeof(t));
    for(size_t i = 0; i < t; ++i) {
        TreeNode *n = new TreeNode();
        in.read((char*) &n->index, sizeof(n->index));
        in.read((char*) &n->label, sizeof(n->label));

        nodes.push_back(n);
        if (n->label >= 0) leaves[n->label] = n;
    }

    int rootN;
    in.read((char*) &rootN, sizeof(rootN));
    root = nodes[rootN];

    for(size_t i = 0; i < t; ++i) {
        TreeNode *n = nodes[i];

        int parentN;
        in.read((char*) &parentN, sizeof(parentN));
        if(parentN >= 0) {
            nodes[parentN]->children.push_back(n);
            n->parent = nodes[parentN];
        }
    }

    std::cerr << "  Nodes: " << nodes.size() << ", leaves: " << leaves.size() << "\n";
}

void Tree::printTree(TreeNode *rootNode){
    if(rootNode == nullptr) rootNode = root;

    std::unordered_set<TreeNode*> nSet;
    std::queue<TreeNode*> nQueue;
    nQueue.push(rootNode);
    nSet.insert(rootNode);
    int depth = 0;

    while(!nQueue.empty()){
        TreeNode* n = nQueue.front();
        nQueue.pop();

        if(nSet.count(n->parent)){
            nSet.clear();
            std::cerr << "\nDepth " << ++depth << ":";
        }

        nSet.insert(n);
        std::cerr << " " << n->index;
        if(n->parent) std::cerr << "(" << n->parent->index << ")";
        for(auto c : n->children) nQueue.push(c);
    }

    std::cerr << "\n";
}

int Tree::numberOfLeaves(TreeNode *rootNode){
    if(rootNode == nullptr) // Root node
        return leaves.size();

    int lCount = 0;
    std::queue<TreeNode*> nQueue;
    nQueue.push(rootNode);

    while(!nQueue.empty()){
        TreeNode* n = nQueue.front();
        nQueue.pop();

        if(n->label >= 0) ++lCount;
        for(auto c : n->children) nQueue.push(c);
    }

    return lCount;
}