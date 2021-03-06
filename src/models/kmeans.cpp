/**
 * Copyright (c) 2018-2020 by Marek Wydmuch
 * All rights reserved.
 */

#include <algorithm>
#include <climits>
#include <cmath>
#include <random>

#include "kmeans.h"
#include "misc.h"

// K-Means clustering with balanced option
// Partition is returned via reference, calculated for cosine distance
void kMeans(std::vector<Assignation>* partition, SRMatrix<Feature>& pointsFeatures, int centroids, double eps,
            bool balanced, int seed) {

    int points = partition->size();
    int features = pointsFeatures.cols();

    // if(balanced) std::cerr << "Balanced K-Means ...\n  Partition: " << partition->size() << ", centroids: " <<
    // centroids << "\n";
    // else std::cerr << "K-Means ...\n  Partition: " << partition->size() << ", centroids: " << centroids << "\n";

    int maxPartitionSize = points - centroids, maxWithOneMore = 0;
    if (balanced) {
        maxPartitionSize = points / centroids;
        maxWithOneMore = points % centroids;
        assert(centroids * maxPartitionSize + maxWithOneMore == partition->size());
    }

    // Init centroids
    std::vector<std::vector<double>> centroidsFeatures(centroids);

    std::default_random_engine rng(seed);
    std::uniform_int_distribution<int> dist(0, points);
    for (int i = 0; i < centroids; ++i) {
        centroidsFeatures[i].resize(features, 0);
        setVector(pointsFeatures[dist(rng)], centroidsFeatures[i]);
    }

    double oldCos = INT_MIN, newCos = -1;

    std::vector<Similarities> similarities(points);
    for (int i = 0; i < points; ++i)
        similarities[i].values.resize(centroids);

    while (newCos - oldCos >= eps) {

        oldCos = newCos;
        newCos = 0;

        if(centroids == 2){ // Faster version for 2-means

            // Calculate similarity to centroids
            for (int i = 0; i < points; ++i) {
                similarities[i].index = i;
                for (int j = 0; j < centroids; ++j) {
                    similarities[i].values[j].index = j;
                    similarities[i].values[j].value = dotVectors(pointsFeatures[(*partition)[i].index], centroidsFeatures[j]);
                }
                similarities[i].sortby = similarities[i].values[0].value - similarities[i].values[1].value;
            }

            // Assign points to centroids and calculate new loss
            std::sort(similarities.begin(), similarities.end());

            for (int i = 0; i < points; ++i) {
                int cIndex;
                if(balanced) cIndex = (i < maxPartitionSize) ? 1 : 0; // If balanced
                else cIndex = (similarities[i].sortby <= 0) ? 1 : 0;
                (*partition)[similarities[i].index].value = cIndex;
                newCos += similarities[i].values[cIndex].value;
            }
        } else {
            std::vector<int> centroidsSizes(centroids, 0);

            for (int i = 0; i < points; ++i) {
                similarities[i].index = i;
                for (int j = 0; j < centroids; ++j) {
                    similarities[i].values[j].index = j;
                    similarities[i].values[j].value = dotVectors(pointsFeatures[(*partition)[i].index], centroidsFeatures[j]);
                }

                std::sort(similarities[i].values.begin(), similarities[i].values.end(),
                        [](const Feature& a, const Feature& b) -> bool
                    {
                        return a.value > b.value;
                    });
                similarities[i].sortby = similarities[i].values[0].value;
            }

            // Assign points to centroids and calculate new loss
            std::sort(similarities.rbegin(), similarities.rend());

            for (int i = 0; i < points; ++i) {
                for (int j = 0; j < centroids; ++j) {
                    int cIndex = similarities[i].values[j].index;
                    int lIndex = similarities[i].index;

                    if (centroidsSizes[cIndex] < maxPartitionSize ||
                        (centroidsSizes[cIndex] < maxPartitionSize + 1 && maxWithOneMore > 0)) {

                        if (centroidsSizes[cIndex] == maxPartitionSize) --maxWithOneMore;

                        (*partition)[lIndex].value = cIndex;
                        ++centroidsSizes[cIndex];
                        newCos += similarities[i].values[j].value;
                        break;
                    }
                }
            }
        }

        newCos /= points;

        // Update centroids
        for (auto& c : centroidsFeatures) std::fill(c.begin(), c.end(), 0);
        for (auto& p : (*partition)) addVector(pointsFeatures[p.index], centroidsFeatures[p.value]);
        for (auto& c : centroidsFeatures) unitNorm(c);
    }

    //std::cerr << Final similarity: << newCos << "\n";
}
