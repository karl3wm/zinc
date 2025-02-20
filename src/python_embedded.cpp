#include <zinc/python.hpp>

#include <Python.h>

namespace {

void interpreter()
{
    static class Interpreter
    {
    public:
        Interpreter()
        {
            // Initialize Python interpreter
            Py_Initialize();
        }
        ~Interpreter()
        {
            // Finalize Python interpreter
            Py_Finalize();
        }
    } interpreter;
}

}

std::string_view zinc::Python::execute(const std::string_view script)
{
    // Initialize Python interpreter
    interpreter();

    // Create a Python code object from the script
    PyObject* pyCode = Py_CompileString(script.data(), "<string>", Py_file_input);

    // Create a Python dictionary to hold the global variables
    static thread_local PyObject* pyGlobals = PyDict_New();

    // Create a Python dictionary to hold the local variables
    static thread_local PyObject* pyLocals = PyDict_New();

    // Execute the Python code
    PyObject* pyResult = PyEval_EvalCode(pyCode, pyGlobals, pyLocals);

    // Get the output of the execution
    static thread_local PyObject* pyOutput = nullptr;
    if (pyOutput != nullptr) {
        Py_DECREF(pyOutput);
    }
    pyOutput = PyObject_GetAttrString(pyGlobals, "__stdout__");
    if (pyOutput == NULL) {
        pyOutput = PyUnicode_FromString("");
    }

    // Convert the output to a C++ string
    char const * output; Py_ssize_t output_size;
    output = PyUnicode_AsUTF8AndSize(pyOutput, &output_size);

    // Clean up
    Py_DECREF(pyCode);
    //Py_DECREF(pyGlobals);
    Py_DECREF(pyResult);

    return {output, (size_t)output_size};
}
