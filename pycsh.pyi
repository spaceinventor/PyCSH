"""
Interface documentation of the libparam Python bindings.

The bindings API largely mimics the shell interface to CSH,
meaning that the same commands (with their respective parameters), are available as functions here.
Wrapper classes for param_t's and a class providing an interface to param_queue_t are also made available.
These provide an object-oriented interface to libparam, but are largely meant for convenience.
"""

from __future__ import annotations

from typing import \
    Any as _Any, \
    Iterable as _Iterable, \
    Literal as _Literal

_param_value_hint = int | float | str
_param_type_hint = _param_value_hint | bytearray

# For consistencies’ sake, we override the builtin list class with the list command from CSH.
# So let's make sure we can still retrieve the original class.
_pylist = type([])

class Parameter:
    """
    Wrapper class for Libparam's parameters.
    Provides an interface to their attributes and values.
    """

    def __new__(cls, param_identifier: _param_ident_hint, node: int = None, host: int = None, timeout: int = None, retries: int = None) -> Parameter | ParameterArray:
        """
        As stated; this class simply wraps existing parameters,
        and cannot create new ones. It therefore requires an 'identifier'
        to the parameter you wish to retrieve.

        :param param_identifier: an int or string of the parameter ID or name respectively.
        :param node: Node on which the parameter is located.
        :param timeout: Default timeout period for this parameter in milliseconds.
        :param retries: Number of retries available for timeouts.

        :raises ValueError: When no parameter can be found from an otherwise valid identifier.

        :returns: An instance of a Parameter or ParameterArray, matching the identifier.
        """

    @property
    def name(self) -> str:
        """ Returns the name of the wrapped param_t C struct. """

    @property
    def unit(self) -> str | None:
        """ The unit of the wrapped param_t c struct as a string or None. """

    @property
    def docstr(self) -> str | None:
        """ The help-text of the wrapped param_t c struct as a string or None. """

    @property
    def id(self) -> int:
        """ Returns the id of the wrapped param_t C struct. """

    @property
    def node(self) -> int:
        """ Returns the node of the parameter, as in; the one in the wrapped param_t C struct. """

    @node.setter
    def node(self, value: int) -> None:
        """
        Attempts to set the parameter to a matching one, on the specified node.

        :param value: Node on which the other parameter ought to be located.

        :raises TypeError: When attempting to delete or assign the node to an invalid type.
        :raises ValueError: When a matching parameter cannot be found on the specified node.
        """

    @property
    def host(self) -> int | None:
        """ Returns the host the parameter uses for operations. """

    @host.setter
    def host(self, value: int | None) -> None:
        """ Naively sets the node used when querying parameters. Uses node when None. """

    @property
    def type(self) -> _param_type_hint:
        """ Returns the best Python representation type object of the param_t c struct type. i.e int for uint32. """

    @property
    def value(self) -> int | float:
        """
        Returns the value of the parameter from its specified node in the Python representation of its type.
        Array parameters return a tuple of values, whereas normal parameters return only a single value.
        """

    @value.setter
    def value(self, value: int | float) -> None:
        """
        Sets the value of the parameter.

        :param value: New desired value. Assignments to other parameters, use their value instead, Otherwise uses .__str__().
        """

    @property
    def is_array(self) -> bool:
        """ Returns True or False based on whether the parameter is an array (Strings count as arrays)."""

    @property
    def is_vmem(self) -> bool:
        """ Returns True or False based on whether the parameter is a vmem parameter. """

    @property
    def mask(self) -> int:
        """ Returns the mask of the wrapped param_t C struct. """

    @property
    def timestamp(self) -> int:
        """ Returns the timestamp of the wrapped param_t C struct. """

    @property
    def timeout(self) -> int:
        """ Returns the default timeout of the Parameter value in milliseconds. """

    @timeout.setter
    def timeout(self, value: int) -> None:
        """
        Sets the default timeout of the Parameter in milliseconds.
        Use None to reset default value.
        """

    @property
    def retries(self) -> int:
        """ Returns the number of retries the parameter has for transactions. """

    @timeout.setter
    def retries(self, value: int) -> None:
        """
        Sets the retries the parameter has for transactions.
        Use None to reset default value.
        """


class ParameterArray(Parameter):
    """
    Subclass of Parameter specifically designed to provide an interface
    to array parameters. In practical terms; the class implements:
        __len__(), __getitem__() and __setitem__().
    The Parameter class will automatically create instances of this class when needed,
    which means that manual instantiation of ParameterArrays is unnecessary.
    """

    def __len__(self) -> int:
        """
        Gets the length of array parameters.

        :raises AttributeError: For non-array type parameters.
        :return: The value of the wrapped param_t->array_size.
        """

    def __getitem__(self, index: int) -> _param_type_hint:
        """
        Get the value of an index in an array parameter.

        :param index: Index on which to get the value. Supports backwards subscription (i.e: -1).
        :raises IndexError: When trying to get a value outside the bounds of the parameter array.
        :raises ConnectionError: When autosend is on, and no response is received.

        :return: The value of the specified index, as its Python type.
        """

    def __setitem__(self, index: int, value: int | float) -> None:
        """
        Set the value of an index in an array parameter.

        :param index: Index on which to set the value. Supports backwards subscription (i.e: -1).
        :param value: New value to set, should match the type of the parameter.
        :raises IndexError: When trying to set value ouside the bounds the parameter array.
        :raises ConnectionError: When autosend is on, and no response is received.
        """

    @property
    def value(self) -> str | tuple[int | float]:
        """
        Returns the value of the parameter from its specified node in the Python representation of its type.
        Array parameters return a tuple of values, whereas normal parameters return only a single value.
        """

    @value.setter
    def value(self, value: str | _Iterable[int | float]) -> None:
        """
        Sets the value of the parameter.

        :param value: New desired value. Assignments to other parameters, use their value instead, Otherwise uses .__str__().
        """


# PyCharm may refuse to acknowledge that a list subclass is iterable, so we explicitly state that it is.
class ParameterList(_pylist[Parameter | ParameterArray], _Iterable):
    """
    Convenience class providing an interface for pulling and pushing the value of multiple parameters
    in a single request using param_queue_t's
    """

    def __init__(self, _: Parameter | _Iterable[Parameter] = None, /, *args: Parameter) -> None:
        """
        Accepts either a single iterable of Parameter objects,
        or Parameter objects directly through *args, as its initial values.
        """

    def append(self, __object: Parameter) -> None:
        """
        Appends the specified Parameter object to the list.

        :raises TypeError: When attempting to append a non-Parameter object.
        """

    def pull(self, host: int, timeout: int = None) -> None:
        """
        Pulls all Parameters in the list as a single request.

        :raises ConnectionError: When no response is received.
        """

    def push(self, node: int, timeout: int = None, hwid: int = None, paramver: int = 2) -> None:
        """
        Pushes all Parameters in the list in a single packet.

        :raises ConnectionError: When no response is received.
        """


_param_ident_hint = int | str | Parameter  # Types accepted for finding a param_t


# Libparam commands
def get(param_identifier: _param_ident_hint, node: int = None, server: int = None, paramver: int = 2, offset: int = None, timeout: int = None, retries: int = None) -> _param_value_hint | tuple[_param_value_hint]:
    """
    Get the value of a parameter.

    :param param_identifier: string name, int id or Parameter object of the desired parameter.
    :param node: node (default = <env>)
    :param server: server to get parameters from (default = node)
    :param paramver: parameter system version (default = 2)
    :param offset: Index to use for array parameters.
    :param timeout: Timeout of pull transaction in milliseconds (Has no effect when autosend is 0).
    :param retries: Number of retries available for timeouts.

    :raises TypeError: When an invalid param_identifier type is provided.
    :raises ValueError: When a parameter could not be found.
    :raises RuntimeError: When called before .init().

    :return: The value of the retrieved parameter (As its Python type).
    """

def set(param_identifier: _param_ident_hint, value: _param_value_hint | _Iterable[int | float], node: int = None, server: int = None, paramver: int = 2, offset: int = None, timeout: int = None, retries: int = None) -> None:
    """
    Set the value of a parameter.

    :param param_identifier: string name, int id or Parameter object of the desired parameter.
    :param value: The new value of the parameter. .__str__() of the provided object will be used.
    :param node: node (default = <env>)
    :param server: server to get parameters from (default = node)
    :param paramver: parameter system version (default = 2)
    :param offset: Index to use for array parameters.
    :param timeout: Timeout of push transaction in milliseconds (Has no effect when autosend is 0).
    :param retries: Number of retries available for timeouts.

    :raises TypeError: When an invalid param_identifier type is provided.
    :raises ValueError: When a parameter could not be found.
    :raises RuntimeError: When called before .init().
    """

def push(node: int, timeout: int = None, hwid: int = None, retries: int = None) -> None:
    """
    Push the current queue.

    :param node: Node to which the current queue should be pushed.
    :param timeout: Timeout in milliseconds of the push request.
    :param hwid: Hardware ID.
    :param retries: Number of retries available for timeouts.

    :raises ConnectionError: when no response is received.
    """

def pull(host: int, include_mask: str = None, exclude_mask: str = None, timeout: int = None, retries: int = None) -> None:
    """ Pull all or a specific set of parameters. """

def cmd_new(type: _Literal['get', 'set'], name: str = None, paramver: int = 2) -> None:
    """
    Create a new global command/queue

    :param type: Whether to create a 'get' or 'set' queue.
    :param name: Name of the new queue.
    :param paramver: parameter system verison (default = 2)

    :raises ValueError: When an incorrect type of queue is specified.
    """

def cmd_done() -> None:
    """ Clears the queue. """

def node(node: int = None) -> int:
    """
    Used to get or change the default node.

    :param node: Integer to change the default node to.
    :return: The current default node.
    """

def timeout(timeout: int = None) -> int:
    """
    Used to get or change the default timeout.

    :param timeout: Integer to change the default timeout to.
    :return: The current default timeout.
    """

def cmd() -> None:
    """ Print the current command. """

def list(node: int = None, verbose: int =None, mask: str = None, globstr: str = None) -> ParameterList:
    """
    List all known parameters, remote and local alike.

    :param mask: Mask on which to filter the list.
    """

def list_download(node: int, timeout: int = None, version: int = None) -> ParameterList:
    """
    Download all parameters on the specified node.

    :raises RuntimeError: When called before .init().
    :raises ConnectionError: When no response is received.

    :returns: The output of list().
    """

def list_forget(node: int = None, verbose: int = None) -> int:
    """
    Remove remote parameters, matching the provided arguments, from the global list.

    :param node: Remove parameters from this node. Use <1 for all nodes.
    :param name_filter: Wildcard name pattern to filter parameters by.
    :returns: Count of parameters affected.
    """


# Commands from CSP
def ping(node: int, timeout: int = None, size: int = None) -> int:
    """
    Ping the specified node.

    :param node: Address of subsystem.
    :param timeout: Timeout in ms to wait for reply.
    :param size: Payload size in bytes.

    :raises RuntimeError: When called before .init().

    :return: >0 = echo time in mS on success, otherwise -1 for error.
    """

def ident(node: int, timeout: int = None) -> str:
    """
    Print the identity of the specified node.

    :param node: Address of subsystem.
    :param timeout: Timeout in ms to wait for reply.

    :raises RuntimeError: When called before .init().
    :raises ConnectionError: When no response is received.
    """

def reboot(node: int) -> None:
    """ Reboot the specified node. """


def get_type(param_identifier: _param_ident_hint, node: int = None) -> _param_type_hint:
    """
    Gets the type of the specified parameter.

    :param param_identifier: string name, int id or Parameter object of the desired parameter.
    :param node: Node of the parameter.
    :return: The best Python representation type object of the param_t c struct type. i.e int for uint32.
    """


# Vmem commands
def vmem(node: int = None, timeout: int = None, version: int = None) -> str:
    """
    Builds a string of the vmem at the specified node.

    :param node: Node from which the vmem should be listed.
    :param timeout: Timeout in ms when connecting to the node.

    :raises RuntimeError: When called before .init().
    :raises ConnectionError: When the timeout is exceeded attempting to connect to the specified node.
    :raises MemoryError: When allocation for a CSP buffer fails.

    :return: The string of the vmem at the specfied node.
    """


def csp_init(host: str = None, model: str = None, revision: str = None, version: int = 2, dedup: int = 3) -> None:
    """
    Initialize CSP

    :param host: Hostname (default = linux hostname)
    :param model: Model name (default = linux get domain name)
    :param revision: Revision (default = release name)
    :param version: CSP version (default = 2)
    :param dedup: CSP dedup 0=off 1=forward 2=incoming 3=all (default)
    """

def csp_add_zmq(addr: int, server: str, promisc: int = 0, mask: int = 8, dfl: int = 0) -> None:
    """
    Add a new ZMQ interface

    :param addr: Node of the interface
    :param server: ZMQProxy hostname/IP
    :param promisc: Promiscuous Mode
    :param mask: Netmask (defaults to 8)
    :param dfl: Set as default
    """

def csp_add_kiss(addr: int, promisc: int = 0, mask: int = 8, dfl: int = 0, baud: int = 1000000, uart: str = "ttyUSB0") -> None:
    """
    Add a new KISS/UART interface

    :param addr: Node of the interface
    :param promisc: Promiscuous Mode
    :param mask: Netmask (defaults to 8)
    :param dfl: Set as default
    :param baud: Baudrate
    :param uart: UART device name (defaults to ttyUSB0)

    :raises SystemError: When failing to add interface
    """

def csp_add_can(addr: int, promisc: int = 0, mask: int = 8, dfl: int = 0, baud: int = 1000000, can: str = "can0") -> None:
    """
    Add a new CAN interface

    :param addr: Node of the interface
    :param promisc: Promiscuous Mode
    :param mask: Netmask (defaults to 8)
    :param dfl: Set as default
    :param baud: Baudrate
    :param can: UART device name (defaults to ttyUSB0)

    :raises SystemError: When failing to add interface
    """

def csp_add_udp(addr: int, server: str, promisc: int = 0, mask: int = 8, dfl: int = 0, listen_port: int = 9220, remote_port: int = 9220) -> None:
    """
    Add a new UDP interface

    :param addr: Node of the interface
    :param server: Parameter server
    :param promisc: Promiscuous Mode
    :param mask: Netmask (defaults to 8)
    :param dfl: Set as default
    :param listen_port: Port to listen on
    :param remote_port: Port to send to
    """

def csp_add_tun(addr: int, tun_src: int, tun_dst: int, promisc: int = 0, mask: int = 8, dfl: int = 0) -> None:
    """
    Add a new TUN interface

    :param addr: Node of the interface
    :param promisc: Promiscuous Mode
    :param mask: Netmask (defaults to 8)
    :param dfl: Set as default
    """

def init(csp_version = None, csp_hostname: str = None, csp_model: str = None,
                use_prometheus: int = None, rtable: str = None, yamlname: str = None, dfl_addr: int = None, quiet: int = None, stdout: int | str = None, stderr: int | str = None) -> None:
    """
    Initializes the module, with the provided settings.

    :param csp_version: Which CSP version to use in the module.
    :param csp_hostname: Which CSP hostname to use in the module.
    :param csp_model: Which CSP model to use in the module.
    :param yamlname: Name and path to the .yaml file with which the bindings/CSH-session should be configured.
    :param dfl_addr: Override CSH' own node with the specified number.
    :param quiet: Send output from C to /dev/null. "quiet" argument takes precedence over the "stdout" & "stderr" arguments.
    :param stdout: Redirect CSH stdout to the specified file. Supports subprocess.DEVNULL for quiet operation.
    :param stderr: Redirect CSH stderr to the specified file. Supports subprocess.DEVNULL for quiet operation.

    :raises ValueError: Incorrect integer value specified for stdout or stderr.
    :raises IOError: When opening a file for stdout or stderr fails.
    :raises RuntimeError: When called multiple times.
    """
