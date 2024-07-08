#include <iostream>
#include <filesystem>
#include <vector>
#include <algorithm>
#include "vk_nv_sharpen.h"

std::vector<std::string> GetImageFilesInDirectory(const std::string& directoryPath)
{
    std::vector<std::string> filePaths;
    for (const auto& entry : std::filesystem::directory_iterator(directoryPath))
    {
        if (entry.is_regular_file())
        {
            std::string extension = entry.path().extension().string();
            std::transform(extension.begin(), extension.end(), extension.begin(), ::tolower);
            if (extension == ".png" || extension == ".jpg" || extension == ".jpeg" || extension == ".bmp")
            {
                filePaths.push_back(entry.path().string());
            }
        }
    }
    return filePaths;
}

std::filesystem::path CreateOutputDirectory(const std::filesystem::path& executablePath)
{
    std::filesystem::path outputDir = executablePath.parent_path() / "output";

    if (!std::filesystem::exists(outputDir))
    {
        if (!std::filesystem::create_directory(outputDir))
        {
            throw std::runtime_error("Failed to create output directory");
        }
    }

    return outputDir;
}

void PrintUsage(const char* programName)
{
    std::cerr << "Usage: " << programName << " <directory_path> [sharpness]" << std::endl;
    std::cerr << "  sharpness: Optional value between 0 and 100 (default is 50)" << std::endl;
}

int main(int argc, char* argv[])
{
    if (argc < 2 || argc > 3)
    {
        PrintUsage(argv[0]);
        return 1;
    }

    std::string directoryPath = argv[1];
    if (!std::filesystem::exists(directoryPath) || !std::filesystem::is_directory(directoryPath))
    {
        std::cerr << "Error: The specified path is not a valid directory." << std::endl;
        return 1;
    }

    float sharpness = 100.0f;  // Default to 100% sharpness
    if (argc == 3)
    {
        try
        {
            sharpness = std::stof(argv[2]);
            if (sharpness < 0.0f || sharpness > 100.0f)
            {
                throw std::out_of_range("Sharpness value out of range");
            }
        }
        catch (const std::exception&)
        {
            std::cerr << "Error: Invalid sharpness value. Must be between 0 and 100." << std::endl;
            return 1;
        }
    }

    std::filesystem::path outputDir;
    try
    {
        outputDir = CreateOutputDirectory(std::filesystem::path(argv[0]));
    }
    catch (const std::exception& e)
    {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    auto* app = new VkNVSharpen();
    app->SetSharpness(sharpness);

    std::vector<std::string> filePaths = GetImageFilesInDirectory(directoryPath);

    if (filePaths.empty())
    {
        std::cout << "No image files found in the specified directory." << std::endl;
        delete app;
        return 0;
    }

    for (const auto& path : filePaths)
    {
        std::cout << "Processing: " << path << std::endl;
        app->ProcessImage(path, outputDir.string());
    }

    delete app;
    return 0;
}