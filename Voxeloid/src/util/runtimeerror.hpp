#pragma once

#define THROW_RUNTIME_ERROR(msg) \
    throw std::runtime_error(__FUNCTION__ ":\n\t" msg);