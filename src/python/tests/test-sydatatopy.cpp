#include <pybind11/embed.h>
#include <pybind11/pybind11.h>

#include <iostream>
#include <sstream>
#include <stdexcept>

#include "sydatatopy.h"

namespace py = pybind11;
using namespace Syntalos;

PYBIND11_EMBEDDED_MODULE(sydatatopy_test_types, m)
{
    py::class_<MetaSize>(m, "MetaSize")
        .def(py::init<>())
        .def(py::init<int32_t, int32_t>())
        .def_readwrite("width", &MetaSize::width)
        .def_readwrite("height", &MetaSize::height);
}

#define TEST_ASSERT(v)                                                     \
    do {                                                                   \
        if (!(v)) {                                                        \
            std::stringstream ss;                                          \
            ss << "TEST_ASSERT failed at " << __FILE__ << ":" << __LINE__; \
            throw std::runtime_error(ss.str());                            \
        }                                                                  \
    } while (false)

static void test_metasize_not_aliased_when_cast_with_reference_policy()
{
    MetaStringMap source;
    source["size"] = MetaSize(640, 480);

    auto pyObjHandle = py::detail::type_caster<MetaStringMap>::cast(
        source, py::return_value_policy::reference_internal, py::none());
    py::dict pyMap = py::reinterpret_steal<py::dict>(pyObjHandle);

    auto &sizeInSource = std::get<MetaSize>(static_cast<MetaValue::Base &>(source["size"]));
    sizeInSource.width = 123;
    sizeInSource.height = 77;

    py::object pySize = pyMap[py::str("size")];
    const int pyWidth = pySize.attr("width").cast<int>();
    const int pyHeight = pySize.attr("height").cast<int>();

    // Python-side MetaSize must be independent from source map storage.
    TEST_ASSERT(pyWidth == 640);
    TEST_ASSERT(pyHeight == 480);
}

int main()
{
    py::scoped_interpreter guard{};

    // Register MetaSize so sydatatopy type casters can create Python MetaSize objects.
    py::module_::import("sydatatopy_test_types");

    test_metasize_not_aliased_when_cast_with_reference_policy();
    std::cout << "sydatatopy tests passed\n";
    return 0;
}
