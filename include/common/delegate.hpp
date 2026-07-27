//
// Created by silay on 6/6/26.
//

#ifndef HFT_SIMULATOR_DELEGATE_HPP
#define HFT_SIMULATOR_DELEGATE_HPP
#include <functional>

template<typename Signature>
class Delegate {
public:
    void Invoke() {
        _callback();
    }

    void SetCallback(std::function<Signature> callback) {
        _callback = callback;
    }

    std::function<Signature> _callback;
};

#endif //HFT_SIMULATOR_DELEGATE_HPP
