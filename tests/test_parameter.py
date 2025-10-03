import pycsh
import unittest
from time import sleep
from contextlib import contextmanager
from typing import NamedTuple, Callable, Any
from pycsh import Parameter, PythonParameter, PARAM_TYPE_STRING, PARAM_TYPE_UINT8, PM_CONF


class ParamArguments(NamedTuple):
    normal_param: Parameter
    array_param: Parameter
    str_param: Parameter


class TestArrayParameter(unittest.TestCase):

    @classmethod
    def setUpClass(cls):
        
        try:
            pycsh.Ifstat("LOOP", node=0)
        except (RuntimeError, ConnectionError):
            pycsh.csp_init()
        
        # Give CSP/ZMQ thread time to start
        sleep(0.1)

        # Set default node to 0
        pycsh.node(0)

    def test_create_param(self) -> PythonParameter:
        """ If this test fails, then we know the act of creating parameters themselves doesn't work.
            Which will cause all other tests to fail. """

        # TODO Kevin: Also test setting remote parameters.
        param_arguments = ParamArguments(
            PythonParameter(300, 'normal_param', PARAM_TYPE_UINT8, PM_CONF, '', 'A new parameter created to be used by tests', 1),
            PythonParameter(301, 'array_param', PARAM_TYPE_UINT8, PM_CONF, '', 'A new parameter created to be used by tests', 8),
            PythonParameter(302, 'str_param', PARAM_TYPE_STRING, PM_CONF, '', 'A new parameter created to be used by tests', 20),
        )

        # Test that parameters were added and can be found in the Parameter linked list.
        #   TODO Kevin: We should also test that they don't have to be,
        #       but we don't have a public API for it yet, even though we already support it.
        for param in param_arguments:
            self.assertIs(param, Parameter(param.name))
            self.assertIs(param, Parameter(param.id))

        # NOTE: This should also be tested.
        param_arguments.array_param.value = (0, 1, 2, 3, 4, 5, 6, 7)

        return param_arguments

    @staticmethod
    def _pass_param_arguments(argument_builder: Callable) -> Callable:
        """ Function which will build arguments for the actual test function. """
        def decorator(func: Callable):
            def wrapper(self, *args, **kwargs) -> Any:
                additional_test_arg = argument_builder(self)
                result = func(self, additional_test_arg, *args, **kwargs)
                return result
            return wrapper
        return decorator

    @contextmanager
    def assertDoesNotRaise(self, exc=Exception):
        try:
            yield
        except exc as e:
            self.fail(f"Unexpected exception raised: {e}")

    @_pass_param_arguments(test_create_param)
    def test_str_param(self, param_args: ParamArguments):

        # Assert defaults value before we mutate it.
        self.assertEqual(param_args.str_param.value, '')
        self.assertEqual(param_args.str_param.value[:], '')


        assert len(param_args.str_param) > 10
        self.assertEqual(param_args.str_param.value[len(param_args.str_param)-5], '')

        new_val: str = 'hello'
        param_args.str_param.value = new_val

        self.assertEqual(param_args.str_param.value, new_val)
        self.assertEqual(param_args.str_param.value[:], new_val)

        # Check that getting outside (behind) of the string value gives an empty string.
        assert len(new_val) < len(param_args.str_param)-5
        self.assertEqual(param_args.str_param.value[len(param_args.str_param)-5], '')

        # Testing custom step-sizes
        self.assertEqual(param_args.str_param.value[::-1], new_val[::-1])
        self.assertEqual(param_args.str_param.value[::2], new_val[::2])

    @_pass_param_arguments(test_create_param)
    def test_param_indexerror(self, param_args: ParamArguments):

        with self.assertRaises(IndexError):
            param_args.array_param.value[len(param_args.array_param)+10]
        with self.assertRaises(IndexError):
            param_args.str_param.value[len(param_args.str_param)+10]

        with self.assertDoesNotRaise(IndexError):
            param_args.str_param.value[:len(param_args.str_param)+10]


    @_pass_param_arguments(test_create_param)
    def test_default_values(self, param_args: ParamArguments):
        for i in range(len(param_args.array_param)):
            self.assertEqual(param_args.array_param.value[i], i)

        # (0, 1, 2, 3, 4, 5, 6, 7)
        self.assertEqual(param_args.array_param.value,       tuple(range(len(param_args.array_param))))  # Default to type of parameter, i.e: tuple[] when `param->array_size > 1`.
        self.assertEqual(param_args.array_param.value[None], tuple(range(len(param_args.array_param))))
        self.assertEqual(param_args.array_param.value[:],    tuple(range(len(param_args.array_param))))

    @_pass_param_arguments(test_create_param)
    def test_slicing(self, param_args: ParamArguments):
        self.assertEqual(param_args.array_param.value[5:-1], (5, 6))
        self.assertEqual(param_args.array_param.value[5:], (5, 6, 7))

    @_pass_param_arguments(test_create_param)
    def test_reverse_slicing(self, param_args: ParamArguments):

        # (7, 6, 5, 4, 3, 2, 1, 0)
        expected = tuple(reversed(range(len(param_args.array_param))))
        self.assertEqual(param_args.array_param.value[::-1], expected)

    @_pass_param_arguments(test_create_param)
    def test_set_value_broadcast(self, param_args: ParamArguments):
        """ Setting without index should set all indices, similar to CSH. """

        assert set(param_args.array_param.value) != {10}
        param_args.array_param.value[:] = 10

        with self.assertRaises(IndexError):
            param_args.array_param.value = 20
        with self.assertRaises(IndexError):
            # Explicit None is also allowed, which is normally not the case with `PyArg_ParseTupleAndKeywords()`.
            #   It is handled as if no index was specified, i.e IndexError for array Parameter.
            param_args.array_param.value[None] = 20
    
        # Check that broadcast set worked, and that IndexErrors didn't also set values. 
        self.assertEqual(param_args.array_param.value, tuple(10 for _ in range(len(param_args.array_param))))

    @_pass_param_arguments(test_create_param)
    def test_invalid_index_type(self, param_args: ParamArguments):
        with self.assertRaises(TypeError):
            param_args.array_param.value["hello index"] = 10


if __name__ == "__main__":
    unittest.main()
