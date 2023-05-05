#pragma once

#include <random>
#include <iostream>
#include <fstream>

/// tests making an rng, generating 10 numbers, serializing it, generating 10 numbers, deserializing it, and seeing if the 10 numbers pass.
void rngTest()
{
    std::mt19937 generator;
    std::uniform_real_distribution<float> distribution(0.0f, 1.0f);

    std::array<float, 10> initialValues = { 0.f };
    std::array<float, 10> secondSequence = { 0.f };
    std::array<float, 10> comparisonSequence = { 0.f };

    for (int i = 0; i < 10; i++)
    {
        initialValues[i] = distribution(generator);
    }

    {
        std::ofstream file("rng.txt");
        file << generator;
        file.close();
    }

    for (int i = 0; i < 10; i++)
    {
        secondSequence[i] = distribution(generator);
    }

    {
        std::ifstream file("rng.txt");
        file >> generator;
        file.close();
    }

    for (int i = 0; i < 10; i++)
    {
        comparisonSequence[i] = distribution(generator);
    }


    if (memcmp((void*)secondSequence.data(), (void*)comparisonSequence.data(), sizeof(comparisonSequence)) == 0)
    {
        std::cout << "RNG Serialization Test PASSED" << std::endl;
    }
    else
    {
        std::cout << "RNG Serialization Test FAILED" << std::endl;
    }

}

