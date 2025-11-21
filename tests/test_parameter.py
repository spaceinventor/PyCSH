import pycsh
import unittest
from time import sleep
from contextlib import contextmanager
from typing import NamedTuple, Callable, Any
from pycsh import Parameter, PARAM_TYPE_STRING, PARAM_TYPE_UINT8, PM_CONF, ValueProxy


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

    def test_create_param(self) -> Parameter:
        """ If this test fails, then we know the act of creating parameters themselves doesn't work.
            Which will cause all other tests to fail. """

        # TODO Kevin: Also test setting remote parameters.
        param_arguments = ParamArguments(
            Parameter.new(300, 'normal_param', PARAM_TYPE_UINT8, PM_CONF, 1, None, '', 'A new parameter created to be used by tests',),
            Parameter.new(301, 'array_param', PARAM_TYPE_UINT8, PM_CONF, 8, None, '', 'A new parameter created to be used by tests',),
            Parameter.new(302, 'str_param', PARAM_TYPE_STRING, PM_CONF, 20, None, '', 'A new parameter created to be used by tests',),
        )

        # Test that parameters were added and can be found in the Parameter linked list.
        #   TODO Kevin: We should also test that they don't have to be,
        #       but we don't have a public API for it yet, even though we already support it.
        for param in param_arguments:
            with self.assertRaises(ValueError):
                Parameter(param.name)
            with self.assertRaises(ValueError):
                Parameter(param.id)

        for param in param_arguments:
            self.assertEqual(param.list_add(False), 0)  # Test that we can successfully add parameters to the list.
            self.assertIs(param.list_add(), Parameter(param.name))
            self.assertIs(param.list_add(), Parameter(param.id))
            # 5 is a unique return value (albeit obtuse) (unlikely to be used by `param_list_add()` in the future),
			#   which indicates that we did not update `self` in the list.
            self.assertEqual(param.list_add(False), 5)
            if param.c_type == PARAM_TYPE_STRING:
                param.list_forget()  # Check that no Valgrind errors occur, whether or not the parameter is in the list.

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
            with self.assertRaises((IndexError, TypeError)):
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

    @_pass_param_arguments(test_create_param)
    def test_callback(self, param_args: ParamArguments):
        param: Parameter = param_args.normal_param
        def invalid_callback() -> None:
            pass
        with self.assertRaises(TypeError):
            param.callback = invalid_callback

        previous_callback = param.callback
        previous_value = int(param.value)

        callback_called: bool = False

        def valid_callback(param: Parameter, index: int) -> None:
            nonlocal callback_called
            callback_called = True
        param.callback = valid_callback
        param.value = 1
        self.assertTrue(callback_called)
        
        param.value = previous_value
        param.callback = previous_callback

    @_pass_param_arguments(test_create_param)
    def test_get(self, param_args: ParamArguments):
        pycsh.set(param_args.str_param, 'hello')
        self.assertEqual(pycsh.get(param_args.str_param, offset=0), 'h',
                         "It should be possible to `pycsh.get()` `PARAM_TYPE_STRING` as well")

        self.assertEqual(pycsh.get(param_args.array_param.name, offset=1), 1,
                         "`pycsh.get(<array_param>, offset=...)` (with explicit index) should return the value on the index, and not as a tuple")
        self.assertEqual(pycsh.get(param_args.array_param.name), tuple(range(8)),
                         "`pycsh.get(<array_param>)` without explicit index should return tuple of whole param")
        

    @_pass_param_arguments(test_create_param)
    def test_list_add(self, param_args: ParamArguments):

        list_param: Parameter = param_args.normal_param

        assert pycsh.Parameter(list_param.id, list_param.node), \
            'Test parameter not in list, please update the test.'

        assert list_param.c_type != pycsh.PARAM_TYPE_UINT16
        original_type: int = list_param.c_type
        new_param: Parameter = pycsh.list_add(list_param.node,
                       len(list_param),
                       list_param.id,
                       list_param.name,
                       pycsh.PARAM_TYPE_UINT16, #list_param.c_type,
                       list_param.mask,
                       list_param.docstr,
                       list_param.unit,
                       )  # Adding a new overriding parameter should update the one in the list, too.

        # Adding it again should not cause memory errors.
        new_param: Parameter = pycsh.list_add(list_param.node,
                       len(list_param),
                       list_param.id,
                       list_param.name,
                       pycsh.PARAM_TYPE_UINT16, #list_param.c_type,
                       list_param.mask,
                       list_param.docstr,
                       list_param.unit,
                       )

        self.assertEqual(list_param.c_type, pycsh.PARAM_TYPE_UINT16)
        self.assertEqual(new_param.c_type, pycsh.PARAM_TYPE_UINT16)
        self.assertEqual(new_param, list_param)

        # Undo type change
        pycsh.list_add(list_param.node,
                       len(list_param),
                       list_param.id,
                       list_param.name,
                       original_type, #list_param.c_type,
                       list_param.mask,
                       list_param.docstr,
                       list_param.unit,
                       )  # Adding a new overriding parameter should update the one in the list, too.


if __name__ == "__main__":
    unittest.main()
