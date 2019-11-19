#pragma once

#include <boost/filesystem.hpp>
#include <cstdio>

struct temp_directory : public boost::filesystem::path {
    temp_directory()
        : path{[] {
              char buffer[L_tmpnam];
              return boost::filesystem::path(std::tmpnam(buffer));
          }()} {
        boost::filesystem::create_directories(*this);
    }

    ~temp_directory() { boost::filesystem::remove_all(*this); }
};
