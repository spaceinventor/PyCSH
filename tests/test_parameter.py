import pycsh
import unittest
from time import sleep
from contextlib import contextmanager
from typing import NamedTuple, Callable, Any
from pycsh import Parameter, PythonParameter, PARAM_TYPE_STRING, PARAM_TYPE_UINT8, PM_CONF, ValueProxy


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


        # NOTE: We now raise an IndexError instead of empty string
        # assert len(param_args.str_param) > 10
        # self.assertEqual(param_args.str_param.value[len(param_args.str_param)-5], '')

        # Test that `Parameter(...)[...]` is a shorthand for `Parameter(...).value[...]`
        proxy_api = lambda : param_args.str_param.value
        param_api  = lambda : param_args.str_param

        for subscript_api in (proxy_api, param_api):
            for new_val in ('hello', 'hi', '123'):
                param_args.str_param.value = new_val

                if isinstance(subscript_api(), ValueProxy):
                    self.assertEqual(subscript_api(), new_val)
                self.assertEqual(subscript_api()[:], new_val)

                # NOTE: We now raise an IndexError instead of empty string
                # # Check that getting outside (behind) of the string value gives an empty string.
                # assert len(new_val) < len(param_args.str_param)-5
                # self.assertEqual(param_args.str_param.value[len(param_args.str_param)-5], '')

                # Testing custom step-sizes
                self.assertEqual(subscript_api()[::-1], new_val[::-1])
                self.assertEqual(subscript_api()[::-2], new_val[::-2])
                self.assertEqual(subscript_api()[::2], new_val[::2])

        # Getting an index outside the string value of the parameter is still an IndexError,
        #   even if it inside the bounds of `len(param)`
        short_str: str = "12"
        param_args.str_param.value = short_str
        for subscript_api in (proxy_api, param_api):
            with self.assertRaises(IndexError):
                subscript_api()[len(short_str)+1]

            with self.assertRaises(IndexError):
                subscript_api()[(i for i in range(len(short_str)+2))]

            with self.assertRaises(IndexError):
                subscript_api()[len(param_args.str_param)+100]

            with self.assertDoesNotRaise(IndexError):
                self.assertEqual(subscript_api()[-1], "2")

            # Just putting this here, to force myself to write unit tests the day we can assign string parameters by index:
            with self.assertRaises(NotImplementedError):
                subscript_api()[4] = "5"

    @_pass_param_arguments(test_create_param)
    def test_param_indexerror(self, param_args: ParamArguments):

        # Test that `Parameter(...)[...]` is a shorthand for `Parameter(...).value[...]`
        proxy_api_arr = lambda : param_args.str_param.value
        param_api_arr  = lambda : param_args.str_param

        for subscript_api in (proxy_api_arr, param_api_arr):
            with self.assertRaises(IndexError):
                subscript_api()[len(param_args.array_param)+10]

        # Test that `Parameter(...)[...]` is a shorthand for `Parameter(...).value[...]`
        proxy_api = lambda : param_args.str_param.value
        param_api  = lambda : param_args.str_param

        for subscript_api in (proxy_api, param_api):
            with self.assertRaises(IndexError):
                subscript_api()[len(param_args.str_param)+10]
            with self.assertDoesNotRaise(IndexError):
                subscript_api()[:len(param_args.str_param)+10]


    @_pass_param_arguments(test_create_param)
    def test_default_values(self, param_args: ParamArguments):

        # Test that `Parameter(...)[...]` is a shorthand for `Parameter(...).value[...]`
        proxy_api = lambda : param_args.array_param.value
        param_api  = lambda : param_args.array_param

        for subscript_api in (proxy_api, param_api):

            for i in range(len(param_args.array_param)):
                self.assertEqual(subscript_api()[i], i)

            # (0, 1, 2, 3, 4, 5, 6, 7)
            if isinstance(subscript_api(), ValueProxy):
                self.assertEqual(param_args.array_param.value,       tuple(range(len(param_args.array_param))))  # Default to type of parameter, i.e: tuple[] when `param->array_size > 1`.
            self.assertEqual(subscript_api()[None], tuple(range(len(param_args.array_param))))
            self.assertEqual(subscript_api()[:],    tuple(range(len(param_args.array_param))))

    @_pass_param_arguments(test_create_param)
    def test_slicing(self, param_args: ParamArguments):
        proxy_api = lambda : param_args.array_param.value
        param_api  = lambda : param_args.array_param

        for subscript_api in (proxy_api, param_api):
            self.assertEqual(subscript_api()[5:-1], (5, 6))
            self.assertEqual(subscript_api()[5:], (5, 6, 7))

    @_pass_param_arguments(test_create_param)
    def test_reverse_slicing(self, param_args: ParamArguments):

        proxy_api = lambda : param_args.array_param.value
        param_api  = lambda : param_args.array_param

        for subscript_api in (proxy_api, param_api):

            # (7, 6, 5, 4, 3, 2, 1, 0)
            expected = tuple(reversed(range(len(param_args.array_param))))
            self.assertEqual(subscript_api()[::-1], expected)

    @_pass_param_arguments(test_create_param)
    def test_set_value_broadcast(self, param_args: ParamArguments):
        """ Setting without index should set all indices, similar to CSH. """

        proxy_api = lambda : param_args.array_param.value
        param_api  = lambda : param_args.array_param

        BROADCAST_VAL: int = 10
        assert set(param_args.array_param.value) != {BROADCAST_VAL}
        for subscript_api in (proxy_api, param_api):

            subscript_api()[:] = BROADCAST_VAL

            if isinstance(subscript_api, ValueProxy):
                with self.assertRaises(IndexError):
                    param_args.array_param.value = BROADCAST_VAL*2
            with self.assertRaises(IndexError):
                # Explicit None is also allowed, which is normally not the case with `PyArg_ParseTupleAndKeywords()`.
                #   It is handled as if no index was specified, i.e IndexError for array Parameter.
                subscript_api()[None] = BROADCAST_VAL*2
    
            # Check that broadcast set worked, and that IndexErrors didn't also set values. 
            self.assertEqual(param_args.array_param.value, tuple(BROADCAST_VAL for _ in range(len(param_args.array_param))))
            self.assertEqual(subscript_api()[:], tuple(BROADCAST_VAL for _ in range(len(param_args.array_param))))

    @_pass_param_arguments(test_create_param)
    def test_invalid_index_type(self, param_args: ParamArguments):
        proxy_api = lambda : param_args.array_param.value
        param_api  = lambda : param_args.array_param
        for subscript_api in (proxy_api, param_api):
            with self.assertRaises(TypeError):
                subscript_api()["hello index"] = 10


if __name__ == "__main__":
    unittest.main()
