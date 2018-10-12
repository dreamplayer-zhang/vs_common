#ifndef __VS_RANDOM_H__
#define __VS_RANDOM_H__
#include <random>
#include "vs_vecutils.h"

namespace vs
{

/** \brief generate a random integer by unform distribution [0, high)*/
int randi(int high);

inline int randInt(int high) {return randi(high);}

/** \brief generate a random integer by unform distribution [low, high)*/
int randi(int low, int high);

inline int randInt(int low, int high) {return randi(low, high);}

/** \brief generate a random float number by unform distribution [0, high)*/
double randf(double high);

inline double randDouble(double high){return randf(high);}

/** \brief generate a random float number by unform distribution [low, high)*/
double randf(double low, double high);

inline double randDouble(double low, double high){return randf(low, high);}

double randn(double mu = 0, double sigma = 1);

std::vector<int> randIntVec(int low, int high, int n);

std::vector<float> randFloatVec(float low, float high, int n);

std::vector<double> randDoubleVec(double low, double high, int n);

std::vector<double> randnVec(double mu, double sigma, int n);

template <typename T>
void shuffleInPlace(std::vector<T>& v, int seed = 0)
{
    std::shuffle(v.begin(), v.end(), std::default_random_engine(seed));  
}

template <typename T>
void aliasTable(const std::vector<T>& weights, std::vector<float>& probs, std::vector<int>& alias)
{
    int N = weights.size();
    float weight_sum = vecSum(weights);
    std::vector<float> norm_probs(N);  //prob * N
    for(int i = 0; i < N; i++)
        norm_probs[i] = (float)weights[i] / weight_sum * (float)N;

    std::vector<int> small_block;
    small_block.reserve(N);
    std::vector<int> large_block;
    large_block.reserve(N);
    for (int k = N - 1; k >= 0; k--)
    {
        if (norm_probs[k] < 1)
            small_block.push_back(k);
        else
            large_block.push_back(k);
    }

    alias.resize(N);
    probs.resize(N);
    while(!small_block.empty() && !large_block.empty())
    {
        int cur_small = small_block.back();
        int cur_large = large_block.back();
        small_block.pop_back();
        large_block.pop_back();
        probs[cur_small] = norm_probs[cur_small];
        alias[cur_small] = cur_large;
        norm_probs[cur_large] = norm_probs[cur_large] + norm_probs[cur_small] - 1;
        if(norm_probs[cur_large] < 1)
            small_block.push_back(cur_large);
        else
            large_block.push_back(cur_large);
    }
    while(!large_block.empty())
    {
        int cur_large = large_block.back();
        large_block.pop_back();
        probs[cur_large] = 1;
    }
    while(!small_block.empty())
    {
        int cur_small = small_block.back();
        small_block.pop_back();
        probs[cur_small] = 1;
    }
}

template <typename T>
std::vector<int> weightedSampleWithReplacement(const std::vector<T>& weights,
                                               int sample_cnt = 1)
{
    std::vector<int> ids;
    ids.reserve(sample_cnt);
    int N = weights.size();
    //Vose's Alias Method
    std::vector<float> probs;
    std::vector<int> alias;
    aliasTable(weights, probs, alias);
    static std::mt19937 mt;
    static std::uniform_real_distribution<float> dis(0.0, 1.0);
    for(int i = 0; i < sample_cnt; i++)
    {
        int k = randi(N);
        if(dis(mt) > probs[k]) k = alias[k];
        ids.push_back(k);
    }
    // std::sort(ids.begin(), ids.end());
    return ids;
}

template <typename T>
std::vector<int> weightedSampleWithoutReplacement(const std::vector<T>& weights,
                                                  int sample_cnt = 1)
{
    std::vector<int> ids;
    ids.reserve(sample_cnt);
    int N = weights.size();
    int n_pos = 0;
    for(auto i : weights) if(i > 0) n_pos++;
    if(n_pos <= sample_cnt)
    {
        for(int i = 0; i < N; i++)
        {
            if(weights[i] > 0) ids.push_back(i);
        }
        return ids;
    }
    //Vose's Alias Method
    std::vector<float> probs;
    std::vector<int> alias;
    aliasTable(weights, probs, alias);
    static std::mt19937 mt;
    static std::uniform_real_distribution<float> dis(0.0, 1.0);

    std::vector<int> select(N, false);
    int select_cnt = 0;
    while(select_cnt < sample_cnt)
    {
        int k = randi(N);
        if(dis(mt) > probs[k]) k = alias[k];
        if(!select[k])
        {
            select[k] = true;
            select_cnt++;
            ids.push_back(k);
        }
    }
    // std::sort(ids.begin(), ids.end());
    return ids;
}

template <typename T>
std::vector<int> weightedSampleLowVariance(const std::vector<T>& weights,
                                           int sample_cnt = 1)
{
    std::vector<int> ids;
    if(sample_cnt <= 0) return ids;
    ids.reserve(sample_cnt);
    /*
        input prob:    |___p1___|_p2_|___p3___|__p4__|...|___pN___|
        uniform samp:  __|__k1__|__k2__|__k3__|__k4__|...|__kM__|__  M is sampleNum
        sample the id in input which the uniform index fall into its prob region.
    */
    int N = weights.size();
    T sum_weight = vecSum(weights);
    double step = sum_weight / sample_cnt;
    if(step <= 0) return ids;
    double r = randf(step * 0.01, step * 0.99);
    double cur_weight = 0;
    for(int i = 0, j = 0; i < sample_cnt; i++, r += step)
    {
        while(j < N && cur_weight < r)
            cur_weight += weights[j++];
        ids.push_back(j-1);
    }
    return ids;
}

/** \brief Importance sampling.
    
    \param[in] weights: weights of probability, must be un-negative number
    \param[in] sample_cnt: sample amount
    \param[in] method: 0 sampling with replacement, uniqueness not ensure, result size is sample_cnt
                       1 sampling without replacement, uniquess ensure, result size is min(sample_cnt, weights_size)
                       2 low variance sampling, uniqueness not ensure, result size is sample_cnt
*/
template <typename T>
std::vector<int> weightedSample(const std::vector<T>& weights,
                                int sample_cnt = 1, int method = 0)
{
    // check all zero
    bool all_zero = true;
    for(auto w : weights)
        if(w > 0)
        {
            all_zero = false;
            break;
        }
    if(all_zero)
    {
        printf("[ERROR]weightedSample: All weights are zero.\n");
        return std::vector<int>();
    }

    switch(method)
    {
        case 0: return weightedSampleWithReplacement(weights, sample_cnt);
        case 1: return weightedSampleWithoutReplacement(weights, sample_cnt);
        case 2: return weightedSampleLowVariance(weights, sample_cnt);
        default:
                printf("[ERROR]weightedSample: Invalid method %d.\n", method);
                return std::vector<int>();
    }
}

} /* namespace vs */
#endif//__VS_RANDOM_H__