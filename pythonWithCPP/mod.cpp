#include <stdio.h>
#include <pybind11/pybind11.h>

// Spew for debugging:
#define _SPEW(file, line, func, fmt, ...)\
         fprintf(stderr, "%s:%d:%s: " fmt "\n", file, line, func, ##__VA_ARGS__)
#define SPEW(...) _SPEW(__BASE_FILE__, __LINE__,__func__,  __VA_ARGS__)



namespace py = pybind11;

// recursive function to print a python object (while there are better ways to
// print, this demonstrates parsing simple or complex python types)
void print_pyobject(py::object o)
{
    if (py::isinstance<py::dict>(o)) {
        size_t i=0, n=py::dict(o).size();
        printf("{");
        for (auto p: py::dict(o)) {
            std::string k = py::cast<std::string>(p.first);
            auto        v = py::cast<py::object> (p.second);
            printf("\"%s\":", k.c_str());
            print_pyobject(py::cast<py::object>(v));
            printf("%s", ++i==n ? "" : ",");
        }
        printf("}");
    } else if (py::isinstance<py::list>(o) || py::isinstance<py::tuple>(o)) {
        size_t i=0, n=py::list(o).size();
        printf("[");
        for (auto v: py::list(o)) {
            print_pyobject(py::cast<py::object>(v));
            printf("%s", ++i==n ? "" : ",");
        }
        printf("]");
    } else if (py::isinstance<py::bool_>(o)) {
        printf("%s", bool(py::bool_(o)) ? "true" : "false");
    } else if (py::isinstance<py::int_>(o)) {
        printf("%d", int(py::int_(o)));
    } else if (py::isinstance<py::float_>(o)) {
        printf("%g", float(py::float_(o)));
    } else if (py::isinstance<py::none>(o)) {
        printf("null");
    } else if (py::isinstance<py::str>(o)) {
        printf("\"%s\"", std::string(py::str(o)).c_str());
    } else {
        printf("?\n");
    }
}

int white(py::object v=py::none())
{
    print_pyobject(v);
    printf("\n");
    return 0;
}

int rabbit(py::args args)
{
    for (auto arg: args) {
        // 'arg' is of type py::handle which for some reason causes
        // problems with casting to specific pybind instances; casting
        // as a py::object fixes this. There are probably better methods...
        print_pyobject( py::cast<py::object>(arg) );
        printf(",");
    }
    printf("\n");
    return 0;
}

// I know this is not the form that I want.
//
// It needs to have generic arguments.  I need to parse the arguments that
// are passed in.   It can easily, but tediously, do that with the python
// C API.
//
// I also need to copy some of the arguments and pass them back to the
// python script as arguments to functions in the python script at a
// later time.  So, this is just the start of what I have not figured out
// how-to ...   It's all very straight forward with the python C API.
//
// Also need one of the passed in arguments to be a python function that
// takes arbitrary arguments. That part I figured out.  The python
// function will be saved and called later; again, very straight forward
// with the python C API.
//
// There are popular python modules that do these kinds of things.  They
// provide very slick interfaces for their users.  Example
// https://flask-socketio.readthedocs.io/en/latest/
//
int magic(int x1, int x2) {
    SPEW("got %d %d", x1, x2);
    return x1 * x2;
}



PYBIND11_MODULE(mod,m) {

    m.doc() = "pybind11 example plugin";

    m.def("magic", &magic, "Magic function");
    m.def("white", &white, "Down the rabbit hole",
          py::arg("v")=py::none());
    m.def("rabbit", &rabbit, "Alice");
}
