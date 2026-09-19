#pragma once

#include <ctime>

namespace clock_service {

class TimeService {
public:
    bool begin();
    bool now(std::tm& local) const;
    bool valid() const { return valid_; }

private:
    bool valid_ = false;
};

}  // namespace clock_service

