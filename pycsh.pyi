"""
Interface documentation of the libparam Python bindings.

The bindings API largely mimics the shell interface to CSH,
meaning that the same commands (with their respective parameters), are available as functions here.
Wrapper classes for param_t's and a class providing an interface to param_queue_t are also made available.
These provide an object-oriented interface to libparam, but are largely meant for convenience.
"""

from __future__ import annotations

from _typeshed import Self as _Self
from types import ModuleType as _ModuleType
from typing import \
    Any as _Any, \
    Iterable as _Iterable, \
    Literal as _Literal, \
    Callable as _Callable
from datetime import datetime as _datetime
from io import IOBase as _IOBase, TextIOBase as _TextIOBase

_param_value_hint = int | float | str
_param_type_hint = _param_value_hint | bytearray

# For consistencies’ sake, we override the builtin list class with the list command from CSH.
# So let's make sure we can still retrieve the original class.
_pylist = type([])

VERSION: str  # Git tag and commit SHA. Format example: 'V1.0-0-gceb4ed4+'
COMPILE_DATE: str  # Date of compilation. Format example: 'Jul 21 2023'
COMPILE_DATETIME: _datetime  # Datetime of compilation.

# Parameter type translated directly from the C enum, these may receive their own Python enum in future versions.
PARAM_TYPE_UINT8: int
PARAM_TYPE_UINT16: int
PARAM_TYPE_UINT32: int
PARAM_TYPE_UINT64: int
PARAM_TYPE_INT8: int
PARAM_TYPE_INT16: int
PARAM_TYPE_INT32: int
PARAM_TYPE_INT64: int
PARAM_TYPE_XINT8: int
PARAM_TYPE_XINT16: int
PARAM_TYPE_XINT32: int
PARAM_TYPE_XINT64: int
PARAM_TYPE_FLOAT: int
PARAM_TYPE_DOUBLE: int
PARAM_TYPE_STRING: int
PARAM_TYPE_DATA: int
PARAM_TYPE_INVALID: int

# Parameter mask translated directly from the C #defines, these may receive their own Python enum in future versions.
PM_READONLY: int
PM_REMOTE: int
PM_CONF: int
PM_TELEM: int
PM_HWREG: int
PM_ERRCNT: int
PM_SYSINFO: int
PM_SYSCONF: int
PM_WDT: int
PM_DEBUG: int
PM_CALIB: int
PM_ATOMIC_WRITE: int
PM_PRIO1: int
PM_PRIO2: int
PM_PRIO3: int
PM_PRIO_MASK: int

VMEM_TYPE_RAM: int
VMEM_TYPE_FRAM: int
VMEM_TYPE_FRAM_SECURE: int
VMEM_TYPE_FLASH: int
VMEM_TYPE_DRIVER: int
VMEM_TYPE_FLASH_QSPI: int
VMEM_TYPE_FILE: int
VMEM_TYPE_FRAM_CACHE: int
VMEM_TYPE_NOR_FLASH: int
VMEM_TYPE_BLOCK: int
VMEM_TYPE_UNKNOWN: int

CSP_NO_VIA_ADDRESS: int = 0xFFFF


# Custom Exceptions
class ProgramDiffError(ConnectionError):
    """
    Raised when a difference is detected between uploaded/downloaded data after programming.
    Must be caught before ConnectionError() baseclass.
    """

class ParamCallbackError(RuntimeError):
    """
    Raised and chains unto exceptions raised in the callbacks of PythonParameters.
    Must be caught before RuntimeError() baseclass.
    """

class InvalidParameterTypeError(ValueError):
    """
    Raised when attempting to create a new PythonParameter() with an invalid type.
    Must be caught before ValueError() baseclass.
    """


class Parameter:
    """
    Wrapper class for Libparam's parameters.
    Provides an interface to their attributes and values.
    """

    name: str  # The name of the wrapped param_t C struct
    unit: str  # The unit of the wrapped param_t c struct as a string or None
    docstr: str  # The help-text of the wrapped param_t c struct as a string or None
    id: int  # ID of the parameter
    type: type  # type of the parameter
    mask: int  # mask of the parameter
    timestamp: int  # timestamp of the parameter
    node: int  # node of the parameter

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
    def host(self) -> int | None:
        """ Returns the host the parameter uses for operations. """

    @host.setter
    def host(self, value: int | None) -> None:
        """ Naively sets the node used when querying parameters. Uses node when None. """

    @property
    def type(self) -> _param_type_hint:
        """ Returns the best Python representation type object of the param_t c struct type. i.e int for uint32. """

    @property
    def cached_value(self) -> int | float:
        """
        Returns the local cached value of the parameter from its specified node in the Python representation of its type.
        Array parameters return a tuple of values, whereas normal parameters return only a single value.
        """

    @cached_value.setter
    def cached_value(self, value: int | float) -> None:
        """
        Sets the local cached value of the parameter.

        :param value: New desired value. Assignments to other parameters, use their value instead, Otherwise uses .__str__().
        """

    @property
    def remote_value(self) -> int | float:
        """
        Returns the remote value of the parameter from its specified node in the Python representation of its type.
        Array parameters return a tuple of values, whereas normal parameters return only a single value.
        """

    @remote_value.setter
    def remote_value(self, value: int | float) -> None:
        """
        Sets the remote value of the parameter.

        :param value: New desired value. Assignments to other parameters, use their value instead, Otherwise uses .__str__().
        """

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

    @retries.setter
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
    def cached_value(self) -> str | tuple[int | float]:
        """
        Returns the local cached value of the parameter from its specified node in the Python representation of its type.
        Array parameters return a tuple of values, whereas normal parameters return only a single value.
        """

    @cached_value.setter
    def cached_value(self, value: str | _Iterable[int | float]) -> None:
        """
        Sets the local cached value of the parameter.

        :param value: New desired value. Assignments to other parameters, use their value instead, Otherwise uses .__str__().
        """

    @property
    def remote_value(self) -> str | tuple[int | float]:
        """
        Returns the remote value of the parameter from its specified node in the Python representation of its type.
        Array parameters return a tuple of values, whereas normal parameters return only a single value.
        """

    @remote_value.setter
    def remote_value(self, value: str | _Iterable[int | float]) -> None:
        """
        Sets the remote value of the parameter.

        :param value: New desired value. Assignments to other parameters, use their value instead, Otherwise uses .__str__().
        """


class PythonParameter(Parameter):
    """ Parameter created in Python. """

    def __new__(cls, id: int, name: str, type: int, mask: int | str, unit: str = None, docstr: str = None, array_size: int = 0,
                   callback: _Callable[[Parameter, int], None] = None, host: int = None, timeout: int = None,
                   retries: int = 0, paramver: int = 2) -> PythonParameter:
        """
        Create a new parameter from the provided options, and add it to the global list.

        :param id: ID of the new parameter.
        :param name: Name of the new parameter.
        :param type: Type of the parameter i.e PARAM_TYPE_UINT8.
        :param mask: Parameter flags, i.e PM_CONF. Multiple flags may be ORed together.
        :param unit: Unit of the new parameter.
        :param docstr: Docstring of the new parameter.
        :param array_size: Array size of the new parameter. Creates a ParameterArray when > 1.
        :param callback: Python function called when the parameter is set, signature: def callback(param: Parameter, offset: int) -> None
        :param host:
        :param timeout: Timeout to use when setting remote parameters.
        :param retries: Amount of retries when setting remote parameters.
        :param paramver: Parameter version to use for this parameter.
        :return: The created Parameter instance.
        """

    @property
    def keep_alive(self) -> bool:
        """
        Whether the Parameter should remain in the parameter list,
        when all Python references are lost.
        This makes it possible to recover the Parameter instance through list()
        """

    @keep_alive.setter
    def keep_alive(self, value: bool) -> None:
        """
        Change whether the Parameter should remain in the parameter list,
        when all Python references are lost.
        """

    @property
    def callback(self) -> _Callable[[Parameter, int], None] | None:
        """
        Callback of the parameter
        """

    @callback.setter
    def callback(self, callback: _Callable[[Parameter, int], None] | None) -> None:
        """
        Change the callback of the parameter
        """

class PythonArrayParameter(PythonParameter, ParameterArray):
    """ ParameterArray created in Python. """

class PythonGetSetParameter(PythonParameter):
    """ ParameterArray created in Python. """

    def __new__(cls, id: int, name: str, type: int, mask: int | str, unit: str = None, docstr: str = None, array_size: int = 0,
                   callback: _Callable[[Parameter, int], None] = None, host: int = None, timeout: int = None,
                   retries: int = 0, paramver: int = 2, getter: _Callable[[Parameter, int], _Any] = None, setter: _Callable[[Parameter, int, _Any], None] = None) -> PythonGetSetParameter:
        """  """

class PythonGetSetArrayParameter(PythonGetSetParameter, PythonArrayParameter):
    """ ParameterArray created in Python. """

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

    def pull(self, node: int = None, timeout: int = None, paramver: int = 2) -> None:
        """
        Pulls all Parameters in the list as a single request.

        :raises ConnectionError: When no response is received.
        """

    def push(self, node: int = None, timeout: int = None, hwid: int = None, paramver: int = 2) -> None:
        """
        Pushes all Parameters in the list in a single packet.

        :raises ConnectionError: When no response is received.
        """


class SlashCommand:
    """ Wrapper class for slash commands """

    @property
    def name(self):
        """ :returns: the full name of the wrapped slash command (including spaces) """

    @property
    def args(self):
        """ :returns: the argument documentation string """

    def __new__(cls: type[_Self], name: str) -> _Self:
        """
        Finds an existing slash command from the provided 'name'

        :param name: Full name of the desired slash command (including spaces for sub commands).

        :raises ValueError: When no slash command can be found from an otherwise valid identifier.

        :returns: An instance of a SlashCommand, matching the identifier, that can be used to call the wrapped command
        """

    def __call__(self, *args, **kwargs) -> _Any:
        """
        Call the wrapped slash command with any number of arguments,
        that are then translated to slash command arguments

        :param *args: Positional arguments converted to arguments for the slash command.
        :param **kwargs: Keyword arguments converted to named arguments for the slash command.

        :raises ValueError: When the slash commands return any error.

        :returns: TODO Kevin:
        """

class PythonSlashCommand(SlashCommand):
    """ Allows for creating new slash commands in Python, that may then later be called from CSH """

    def __new__(cls: type[_Self], name: str, function: _Callable[..., _Any], args: str = None, short_opts: bool = True) -> _Self:
        """
        Finds an existing slash command from the provided 'name'

        :param name: Full name of the slash command (including spaces for sub commands).
        :param function: Function that the slash command will invoke.
            The function parameters will be converted to optparse options, using type-hints to cast the user-provided values.
        :param args: Optional argument documentation string.
        :param short_opts: Whether to generate short opts for the parameters of the provided function.
            Taking the following signature as an example:
            `def function(option: str) -> None`
            short_opts allows `option` to also be filled by `-o`, otherwise only --option is allowed.
            short_opts also enables exceptions for multiple parameters starting with the same (case-sensitive) letter:
            `def function(option: str, option2: str) -> None` <-- exception
            `def function(option: str, Option2: str) -> None` <-- permitted, as `option2` becomes `-O` */


        :raises TypeError: When arguments are of invalid types.

        :returns: An instance of a PythonSlashCommand,
            that can be used to call the provided function (either from CSH or here)
        """

    @property
    def keep_alive(self) -> bool:
        """
        Whether the slash command should remain in the slash command list,
        when all Python references are lost.
        This makes it possible to call the command from CSH (or similar)
        after the Python script has finished
        """

    @keep_alive.setter
    def keep_alive(self, value: bool) -> None:
        """
        Change whether the slash command should remain in the slash command list,
        when all Python references are lost.
        """


class Ident:
    """
    Convenient wrapper class for 'ident' replies. Allows for easy iteration of multiple responses:

    for reply in Ident(16383):
        print(f"{reply.hostname}@{reply.node}")
    """

    node: int
    "Node of the 'module' that replied, which may differ from the targeted node in case of broadcast"
    hostname: str
    model: str
    revision: str
    "Revision of the 'module' that replied, which typically indicates software version"

    # CSP doesn't seem to require these dates to be compilation dates,
    # but they always seem to be so in practice.
    date: str
    "Typically compilation date, i.e: 'Jun 18 2024'"
    time: str
    "Typically compilation time, i.e: '14:06:09'"
    datetime: _datetime
    "Datetime object constructed from: Ident.date + Ident.time"

    def __new__(cls, node: int = None, timeout: int = None, override: bool = False) -> tuple[Ident]:
        """
        Provide a node to 'ident' and receive an iterable of all replies.

        :param node: Address of which to request identity, defaults to environment node.
        :param timeout: Timeout in ms to wait for reply.
        :param override: Whether to override the "known hosts" hostname of the responding module.

        :raises RuntimeError: When called before .init().

        :returns: A list of Ident instances based on replies to the specified node (which may be a broadcast)
        """

    def __str__(self) -> str:
        """ Will return a string formatted as slash will print an ident reply """

    def __hash__(self) -> int:
        """ Uses all Ident fields to generate a hash """


class Ifstat:
    """Convenient wrapper class for 'ifstat' replies."""

    interface: str
    "interface of the ifstat reply"
    tx: int
    "tx of the ifstat reply"
    rx: int
    "rx of the ifstat reply"
    tx_error: int
    "tx_error of the ifstat reply"
    rx_error: int
    "rx_error of the ifstat reply"
    drop: int
    "drop of the ifstat reply"
    autherr: int
    "autherr of the ifstat reply"
    frame: int
    "frame of the ifstat reply"
    txbytes: int
    "txbytes of the ifstat reply"
    rxbytes: int
    "rxbytes of the ifstat reply"

    def __new__(cls, if_name: str, node: int = None, timeout: int = None) -> Ifstat:
        """
        Potentially makes a blocking request to a remote node.

        :param if_name: Name of the interface to query.
        :param node: Node on which the interface is located.
        :param timeout: Timeout for remote node requests.

        :raises RuntimeError: When called before .init().
        :raises ConnectionError: When no response is received.

        :return: Ifstat object with fields indicating status.
        """


class Vmem:
    """ Convenient wrapper class for 'vmem' replies. Requests all VMEM areas on the specified node. """

    vaddr: int
    "Starting address of the VMEM area. Used for upload and download I think"
    size: int
    "Size of the VMEM area in bytes"
    vmem_id: int
    "ID of the VMEM area, used for certain commands"
    type: int
    "int type of the VMEM area"
    name: str
    "Name of the VMEM area"

    def __new__(cls, node: int = None, timeout: int = None, version: int = 2, verbose: int = None) -> tuple[Vmem]:
        """
        Potentially makes a blocking request to a remote node.

        :param node: Node on which the interface is located.
        :param timeout: Timeout for remote node requests.
        :param version: Version to use when parsing the reply.
            There is no version negotiation, so the reply will be garbled if you get this wrong!
        :param verbose: Whether to print like CSH when requesting.

        :raises RuntimeError: When called before .init().
        :raises ConnectionError: When no response is received.

        :return: Tuple of Vmem area object instances.
        """


_param_ident_hint = int | str | Parameter  # Types accepted for finding a param_t


# Libparam commands
def get(param_identifier: _param_ident_hint, node: int = None, server: int = None, paramver: int = 2, offset: int = None, timeout: int = None, retries: int = None) -> _param_value_hint | tuple[_param_value_hint, ...]:
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

def set(param_identifier: _param_ident_hint, value: _param_value_hint | _Iterable[int | float], node: int = None, server: int = None, paramver: int = 2, offset: int = None, timeout: int = None, retries: int = None, verbose: int = 2) -> None:
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

def pull(node: int = None, timeout: int = None, include_mask: str | int = None, exclude_mask: str | int = None, paramver: int = None) -> None:
    """ Pull all or a specific mask of parameters. """

def slash_execute(command: str) -> int:
    """ Execute string as a slash command. Used to run .csh scripts """

def cmd_new(type: _Literal['get', 'set'], name: str = None, paramver: int = 2) -> None:
    """
    Create a new global command/queue

    :param type: Whether to create a 'get' or 'set' queue.
    :param name: Name of the new queue.
    :param paramver: parameter system version (default = 2)

    :raises ValueError: When an incorrect type of queue is specified.
    """

def cmd_done() -> None:
    """ Clears the queue. """

def node(node: int | str = None) -> int:
    """
    Used to get or change the default node.

    :param node: Integer to change the default node to.
    :return: The current default node.
    """

def timeout(timeout: int = None, verbose: int = None) -> int:
    """
    Used to get or change the default timeout.

    :param timeout: Integer to change the default timeout to.
    :param verbose: >=1 print when setting, >=2 also print when getting
    :return: The current/new default timeout.
    """

def verbose(verbose: int = None) -> int:
    """
    Used to get or change the default parameter verbosity.

    :param verbose: Integer to change the default verbosity to (initial value = -1).
    :return: The current default timeout.
    """

def cmd() -> None:
    """ Print the current command. """

def list(node: int = None, verbose: int = None, mask: str | int = None, globstr: str = None) -> ParameterList:
    """
    List all known parameters, remote and local alike.

    :param mask: Mask on which to filter the list.
    """

def list_download(node: int = None, timeout: int = None, version: int = None) -> ParameterList:
    """
    Download all parameters on the specified node.

    :raises RuntimeError: When called before .init().
    :raises ConnectionError: When no response is received.

    :returns: ParameterList() of the specified node, after download.
    """

def list_forget(node: int = None, verbose: int = None) -> int:
    """
    Remove remote parameters, matching the provided arguments, from the global list.

    :param node: Remove parameters from this node. Use <1 for all nodes.
    #:param name_filter: Wildcard name pattern to filter parameters by.

    :returns: Count of parameters removed.
    """

def list_save(filename: str = None, node: int = None, skip_node: bool = False) -> None:
    """
    Save a list of parameters to a file

    :param filename: Name of file to save "list add" commands to, defaults to stdout.
    :param node: Save parameters on the specified node, use <1 for all nodes.
    :param skip_node: Whether to include the node of the parameter in the "last add" command.

    :returns: Currently returns None, may return string of "list add" commands in future versions.
    """

def list_add(node: int, length: int, id: int, name: str, type: int, mask: int | str = None,  comment: str = None, unit: str = None) -> Parameter:
    """
    Add a parameter to the global list

    :param node: Node ID for the parameter.
    :param length: Length of the parameter.
    :param id: ID for the parameter.
    :param name: Name of the parameter.
    :param type: Type of the parameter.
    :param mask: Mask for the parameter.
    :param comment: Additional comment for the parameter.
    :param unit: Unit of measurement for the parameter.
    :returns: Returns the created parameter.
    """

# Commands from CSP
def ping(node: int = None, timeout: int = None, size: int = None) -> int:
    """
    Ping the specified node.

    :param node: Address of subsystem.
    :param timeout: Timeout in ms to wait for reply.
    :param size: Payload size in bytes.

    :raises RuntimeError: When called before .init().

    :return: >0 = echo time in mS on success, otherwise -1 for error.
    """

def ident(node: int = None, timeout: int = None, override: bool = False) -> str:
    """
    Print the identity of the specified node.

    :param node: Address of which to request identity, defaults to environment node.
    :param timeout: Timeout in ms to wait for reply.
    :param override: Whether to override the "known hosts" hostname of the responding module.

    :raises RuntimeError: When called before .init().
    """

def ifstat(if_name: str, node: int = None, timeout: int = None) -> Ifstat:
    """
    Return information about the specified interface.
    Potentially makes a blocking request to a remote node.

    :param if_name: Name of the interface to query.
    :param node: Node on which the interface is located.
    :param timeout: Timeout for remote node requests.

    :raises RuntimeError: When called before .init().
    :raises ConnectionError: When no response is received.

    :return: Ifstat object with fields indicating status
    """

def reboot(node: int = None) -> None:
    """ Reboot the specified node. """

def uptime(node: int = None) -> int:
    """ Return the uptime for the given node """

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

def vmem_download(address: int = None, length: int = None, node: int = None, window: int = None, conn_timeout: int = None, packet_timeout: int = None, ack_timeout: int = None, timeout: int = None, version: int = 1, use_rdp: bool = True) -> bytes:
    """
    Downloads a VMEM memory area specified by the argument, and puts it in data_out

    :param address: The VMEM address to download from
    :param length: Number of bytes to download
    :param node: Node from which the vmem should be listed.
    :param window: RDP Window.
    :param timeout: Timeout in ms when connecting to the node.

    :raises RuntimeError: When called before .init().
    :raises ConnectionError: When the timeout is exceeded attempting to connect to the specified node.
    :raises MemoryError: When allocation for a CSP buffer fails.

    :return: A series of bytes downloaded from the VMEM area
    """

def vmem_upload(address: int = None, data_in: bytes | _IOBase = None, node: int = None, window: int = None, conn_timeout: int = None, packet_timeout: int = None, ack_timeout: int = None, ack_count: int = None, version: int = 2) -> None:
    """
    Uploads data from data_in to a VMEM memory area specified by the argument.

    :param address: The VMEM address to upload to
    :param data_in: The bytes to upload
    :param node: Node from which the vmem should be listed.
    :param timeout: Timeout in ms when connecting to the node.

    :raises RuntimeError: When called before .init().
    :raises ConnectionError: When the timeout is exceeded attempting to connect to the specified node.
    :raises MemoryError: When allocation for a CSP buffer fails.
    """

def switch(slot: int, node: int = None, times: int = None) -> None:
    """
    Reboot into the specified firmware slot.

    :param slot: Flash slot to boot into.
    :param node: Node to reboot.
    :param times: number of times to boot into this slot (default = 1).

    :raises ConnectionError: When the system cannot be pinged after reboot.
    """

def program(slot: int, filename: str, node: int = None, window: int = None, conn_timeout: int = None, packet_timeout: int = None, ack_timeout: int = None, ack_count: int = None) -> None:
    """
    Upload new firmware to a module.

    :param slot: Flash slot to upload to, cannot be the currently booted one.
    :param filename: firmware .bin file to upload.
    :param node: Node of the module to upload to.
    :param window: rdp window (default = 3 packets)
    :param conn_timeout: rdp connection timeout (default = 10 seconds)
    :param packet_timeout: rdp packet timeout (default = 5 seconds)
    :param ack_timeout: rdp max acknowledgement interval (default = 2 seconds)
    :param ack_count: rdp ack for each (default = 2 packets)

    :raises IOError: When in invalid filename is specified.
    :raises ProgramDiffError: See class docstring.
    :raises ConnectionError: When no connection to the specified node can be established.
    """

def sps(from: int, to: int, filename: str, node: int = None, window: int = None, conn_timeout: int = None, packet_timeout: int = None, ack_timeout: int = None, ack_count: int = None) -> None:
    """
    Switch -> Program -> Switch

    :param from: Flash slot to program from.
    :param to: Flash slot to program.
    :param filename: firmware .bin file to upload.
    :param node: Node of the module to upload to.
    :param window: rdp window (default = 3 packets)
    :param conn_timeout: rdp connection timeout (default = 10 seconds)
    :param packet_timeout: rdp packet timeout (default = 5 seconds)
    :param ack_timeout: rdp max acknowledgement interval (default = 2 seconds)
    :param ack_count: rdp ack for each (default = 2 packets)

    :raises ProgramDiffError: See class docstring.
    """

def apm_load(path: str = '~/.local/lib/csh/', filename: str = None, stop_on_error: bool = False, verbose: int = ...) -> dict[str, _ModuleType | Exception]:
    """
    Loads both .py and .so APMs

    Python built in 'import <apm>' machinery cannot load .so APMs that attempt to link with (Py)CSH.

    :param path: Directory to scan for APMs to load
    :param filename: Specific file/APM to load in the 'path' directory.
    :param stop_on_error: Whether to stop and raise an exception when an APM fails to load.
    :param verbose: How much to print, defaults to environment value.

    :raises Exception: When stop_on_error==True and an APM fails to import.

    :return: A dictionary with str paths of the modules as keys,
        and the APM modules themselves as values (when loaded successfully, otherwise exception raised).
    """

def vm_start(api_root: str, logfile: bool = False, username: str = None, password: str = None, auth_file: str = None, ssl: bool = False, server_port: int = None, skip_verify: bool = False, verbose: bool = False) -> None:
    """
    Start logging to Victoria Metrics
    
    :param api_root: Typically hostname of VM server
    :param logfile: Enable logging to param_sniffer.log
    :param username: Username for vmauth. Must be combined with password.
    :param password: Password for vmauth.
    :param auth_file: File containing private key for zmqproxy.
    :param ssl: Use SSL/TLS
    :param server_port: Overwrite default port
    :param skip_verify: Skip verification of the server's cert and hostname
    :param verbose: Verbose connect
    """

def vm_stop() -> None:
    """
    Stop logging to Victoria Metrics.
    
    Always succeds, no arguments.
    Can be called multiple times without exception. 
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

def csp_add_zmq(addr: int, server: str, promisc: int = 0, mask: int = 8, default: int = 0, pub_port: int = 6000, sub_port: int = 7000, sec_key: str|_TextIOBase|None = None) -> None:
    """
    Add a new ZMQ interface

    :param addr: Node of the interface
    :param server: ZMQProxy hostname/IP
    :param promisc: Promiscuous Mode
    :param mask: Netmask (defaults to 8)
    :param default: Set as default
    :param pub_port: Port to publish on
    :param sub_port: Port to subscribe on
    :param sec_key: Auth key for zmqproxy
    """

def csp_add_kiss(addr: int, mask: int = 8, default: int = 0, baud: int = 1000000, uart: str = "ttyUSB0") -> None:
    """
    Add a new KISS/UART interface

    :param addr: Node of the interface
    :param promisc: Promiscuous Mode
    :param mask: Netmask (defaults to 8)
    :param default: Set as default
    :param baud: Baudrate
    :param uart: UART device name (defaults to ttyUSB0)

    :raises SystemError: When failing to add interface
    """

def csp_add_can(addr: int, promisc: int = 0, mask: int = 8, default: int = 0, baud: int = 1000000, can: str = "can0") -> None:
    """
    Add a new CAN interface

    :param addr: Node of the interface
    :param promisc: Promiscuous Mode
    :param mask: Netmask (defaults to 8)
    :param default: Set as default
    :param baud: Baudrate
    :param can: UART device name (defaults to ttyUSB0)

    :raises SystemError: When failing to add interface
    """

def csp_add_eth(addr: int, device: str = "e", promisc: int = 0, mask: int = 8, default: int = 0, mtu: int = 1200) -> None:
    """
    Add a new ethernet interface

    :param addr: Node of the interface
    :param device: Ethernet device
    :param promisc: Promiscuous Mode
    :param mask: Netmask (defaults to 8)
    :param default: Set as default
    :param mtu: Maximum Transmission Unit
    """

def csp_add_udp(addr: int, server: str, promisc: int = 0, mask: int = 8, default: int = 0, listen_port: int = 9220, remote_port: int = 9220) -> None:
    """
    Add a new UDP interface

    :param addr: Node of the interface
    :param server: Parameter server
    :param promisc: Promiscuous Mode
    :param mask: Netmask (defaults to 8)
    :param default: Set as default
    :param listen_port: Port to listen on
    :param remote_port: Port to send to
    """

def csp_add_tun(addr: int, tun_src: int, tun_dst: int, promisc: int = 0, mask: int = 8, default: int = 0) -> None:
    """
    Add a new TUN interface

    :param addr: Node of the interface
    :param promisc: Promiscuous Mode
    :param mask: Netmask (defaults to 8)
    :param default: Set as default
    """

def csp_add_route(addr: int, mask: int, interface: str, via: int = CSP_NO_VIA_ADDRESS) -> None:
    """
    Add a new route
    
    :param addr: Network address of the target subnet
    :param mask: Subnet mask of the target subnet
    :param interface: Which of our interfaces to use when routing to the specified subnet (i.e ZMQ0, CAN1, etc.)
    :param via: Via address (whatever that is)
    """

def init(quiet: int = None, stdout: int | str = None, stderr: int | str = None) -> None:
    """
    Initializes the module, with the provided settings.

    :param quiet: Send output from C to /dev/null. "quiet" argument takes precedence over the "stdout" & "stderr" arguments.
    :param stdout: Redirect CSH stdout to the specified file. Supports subprocess.DEVNULL for quiet operation.
    :param stderr: Redirect CSH stderr to the specified file. Supports subprocess.DEVNULL for quiet operation.

    :raises ValueError: Incorrect integer value specified for stdout or stderr.
    :raises IOError: When opening a file for stdout or stderr fails.
    :raises RuntimeError: When called multiple times.
    """
