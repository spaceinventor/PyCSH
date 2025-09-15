import pycsh
import unittest
from time import sleep
from pycsh import Parameter


class TestArrayParameter(unittest.TestCase):
    
    param: Parameter
    
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

        # Default value is (0, 1, 2, 3, 4, 5, 6, 7)
        cls.param = Parameter("test_array_param", node=0)

    def test_default_values(self):
        for i in range(len(self.param)):
            self.assertEqual(self.param.get_value(i), i)

        self.assertEqual(self.param.get_value_array(), tuple(range(len(self.param))))
        self.assertEqual(self.param.get_value_array(None), tuple(range(len(self.param))))

    def test_slicing(self):
        self.assertEqual(self.param.get_value_array()[5:-1], (5, 6))
        self.assertEqual(self.param.get_value_array()[5:], (5, 6, 7))

    def test_reverse_slicing(self):
        expected = tuple(reversed(range(len(self.param))))
        self.assertEqual(self.param.get_value_array()[::-1], expected)

    def test_set_value_broadcast(self):
        # This mutates state: sets all elements to 10
        self.param.set_value(10)
        self.param.set_value(10, None)
        self.assertEqual(self.param.get_value_array(), tuple(10 for _ in range(len(self.param))))

    def test_invalid_index_type(self):
        with self.assertRaises(TypeError):
            self.param.set_value(10, "hello index")


if __name__ == "__main__":
    unittest.main()
