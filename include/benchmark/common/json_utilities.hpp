//
// Created by silay on 5/26/26.
//

#ifndef HFT_SIMULATOR_JSON_UTILITIES_HPP
#define HFT_SIMULATOR_JSON_UTILITIES_HPP
#include <iosfwd>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

namespace benchmark {
    template<typename T>
    void write_json_result(
        const T &result,
        std::ostream &output
    ) {
        json result_json = result;

        output
                << result_json.dump()
                << '\n';
    }
}


#endif //HFT_SIMULATOR_JSON_UTILITIES_HPP
