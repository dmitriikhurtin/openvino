// Copyright (C) 2021 Intel Corporation
// SPDX-License-Identifier: Apache-2.0
//

#include "functional_test_utils/layer_test_utils/external_network_tool.hpp"
#include "common_test_utils/file_utils.hpp"

#include "../../frontend/frontend_manager/include/frontend_manager/frontend_manager.hpp"

using namespace LayerTestsUtils;

#ifdef _WIN32
# define getpid _getpid
#endif

#define path_delimiter "/"
#ifdef _WIN32
#define path_delimiter "\\"
#endif

ExternalNetworkTool *ExternalNetworkTool::p_instance = nullptr;
ExternalNetworkMode ExternalNetworkTool::mode = ExternalNetworkMode::DISABLED;
const char *ExternalNetworkTool::modelsPath = "";
ExternalNetworkToolDestroyer ExternalNetworkTool::destroyer;

ExternalNetworkToolDestroyer::~ExternalNetworkToolDestroyer() {
    delete p_instance;
}

void ExternalNetworkToolDestroyer::initialize(ExternalNetworkTool *p) {
    p_instance = p;
}

void ExternalNetworkTool::writeToHashMap(const std::string &network_name,
                                                const std::string &hash) {
    std::ofstream hash_map_file;
    std::string file_path;
    if (*modelsPath != '\0') {
        file_path += std::string(modelsPath) + path_delimiter + "hashMap.txt";
    } else {
        file_path += "hashMap.txt";
    }

    hash_map_file.open(file_path,  std::ios::out | std::ios::app);
    hash_map_file << "{\n";
    hash_map_file << "  \"test\": \"" << network_name << "\",\n";
    hash_map_file << "  \"hash\": \"" << hash         << "\",\n";
    hash_map_file << "},\n";
    hash_map_file.close();
}

ExternalNetworkTool &ExternalNetworkTool::getInstance() {
    if (!p_instance) {
        p_instance = new ExternalNetworkTool();
        destroyer.initialize(p_instance);
    }
    return *p_instance;
}

template <typename T>
std::vector<std::shared_ptr<ov::Node>> ExternalNetworkTool::topological_name_sort(T root_nodes) {
    std::vector<std::shared_ptr<ov::Node>> results = ngraph::topological_sort<T>(root_nodes);

    auto node_comaparator = [](std::shared_ptr<ov::Node> node_left,
                               std::shared_ptr<ov::Node> node_right) {
        std::string name_left = node_left->get_friendly_name();
        std::string name_right = node_right->get_friendly_name();
        bool res = std::strcmp(name_left.c_str(), name_right.c_str());
        return res;
    };

    std::sort(results.begin(), results.end(), node_comaparator);
    return results;
}

void ExternalNetworkTool::updateFunctionNames(std::shared_ptr<ngraph::Function> network) const {
    auto rename = [](std::shared_ptr<ov::Node> node) {
        std::string id   {std::to_string(node->get_instance_id())};
        std::string type {node->get_type_name()};

        std::string new_name = type + "_" + id;

        node->set_friendly_name(new_name);
    };

    for (auto node : network->get_ordered_ops()) {
        rename(node);
    }
}

void ExternalNetworkTool::dumpNetworkToFile(const std::shared_ptr<ngraph::Function> network,
                                            const std::string &network_name) const {
    auto exportPathString = std::string(modelsPath);
    auto hashed_network_name = "network_" + generateHashName(network_name);

    std::string out_xml_path = exportPathString
                                + (exportPathString.empty() ? "" : path_delimiter)
                                + hashed_network_name + ".xml";
    std::string out_bin_path = exportPathString
                                + (exportPathString.empty() ? "" : path_delimiter)
                                + hashed_network_name + ".bin";

    // network->set_topological_sort(topological_name_sort<std::vector<std::shared_ptr<ov::Node>>>);
    ngraph::pass::Manager manager;
    manager.register_pass<ngraph::pass::Serialize>(out_xml_path, out_bin_path);
    manager.run_passes(network);
    // network->set_topological_sort(ngraph::topological_sort<std::vector<std::shared_ptr<ov::Node>>>);
    printf("Network dumped to %s\n", out_xml_path.c_str());
    writeToHashMap(network_name, hashed_network_name);
}

// static ngraph::frontend::FrontEndManager& get_frontend_manager() {
//     static ngraph::frontend::FrontEndManager manager;
//     return manager;
// }

// std::shared_ptr<ngraph::Function> ExternalNetworkTool::loadNetworkFromFile(const std::string &network_name) const {
//     auto importPathString = std::string(modelsPath);
//     auto hashed_network_name = "network_" + generateHashName(network_name);

//     std::string out_xml_path = importPathString
//                                 + (importPathString.empty() ? "" : path_delimiter)
//                                 + hashed_network_name + ".xml";
//     std::string out_bin_path = importPathString
//                                 + (importPathString.empty() ? "" : path_delimiter)
//                                 + hashed_network_name + ".bin";

    // auto& manager = get_frontend_manager();
    // ngraph::frontend::FrontEndManager manager;
    // ngraph::frontend::FrontEnd::Ptr FE;
    // ngraph::frontend::InputModel::Ptr inputModel;
    // FE = manager.load_by_model(out_xml_path, out_bin_path);
    // if (FE)
    //     inputModel = FE->load(out_xml_path, out_xml_path);
    // if (!inputModel) {
    //     IE_THROW(NetworkNotRead) << "Unable to read the model.";
    // }
    // auto network = FE->convert(inputModel);
    // updateFunctionNames(network);
    // printf("Network loaded from %s\n", out_xml_path.c_str());
    // return network;
// }

InferenceEngine::CNNNetwork ExternalNetworkTool::loadNetworkFromFile(const std::shared_ptr<InferenceEngine::Core> core,
                                                                     const std::string &network_name) const {
    auto importPathString = std::string(modelsPath);
    auto hashed_network_name = "network_" + generateHashName(network_name);

    std::string out_xml_path = importPathString
                                + (importPathString.empty() ? "" : path_delimiter)
                                + hashed_network_name + ".xml";
    std::string out_bin_path = importPathString
                                + (importPathString.empty() ? "" : path_delimiter)
                                + hashed_network_name + ".bin";

    auto network = core->ReadNetwork(out_xml_path, out_bin_path);
    printf("Network loaded from %s\n", out_xml_path.c_str());
    return network;
}
