import pycsh
import unittest
from time import sleep
from contextlib import contextmanager


class TestVmem(unittest.TestCase):

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

    @contextmanager
    def assertDoesNotRaise(self, exc=Exception):
        try:
            yield
        except exc as e:
            self.fail(f"Unexpected exception raised: {e}")

    def test_vmem_download(self):

        non_existent_node: int = 1000
        assert pycsh.ping(non_existent_node) < 0

        with self.assertRaises(ConnectionError):
            pycsh.vmem_download(0x0, 10, node=non_existent_node)

    def test_vmem_upload(self):

        non_existent_node: int = 1000
        assert pycsh.ping(non_existent_node) < 0

        with self.assertRaises(ConnectionError):
            pycsh.vmem_upload(0x0, b'000000', node=non_existent_node)


if __name__ == "__main__":
    unittest.main()
