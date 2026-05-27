//
// Created by silay on 5/26/26.
//

#ifndef HFT_SIMULATOR_SIMULATOR_HPP
#define HFT_SIMULATOR_SIMULATOR_HPP
#include <cstdint>

namespace sim {
    enum class EInputType: uint8_t {
        File = 1,
        Socket = 2
    };

    class Simulator {
    public:
        Simulator(EInputType input_type) : input_type(input_type) {
        }

        ~Simulator();

        void start();

        void stop();

    private:
        EInputType input_type;
    };
}

#endif //HFT_SIMULATOR_SIMULATOR_HPP
