import enum
from typing import Dict, Any, Union

from .errors import *


class DeviceException(Exception):  # pylint: disable=too-few-public-methods
    exc: Dict[int, Any] = {
        0x6982: DenyError,
        0x6A86: WrongP1P2Error,
        0x6A87: WrongDataLengthError,
        0x6D00: InsNotSupportedError,
        0x6E00: ClaNotSupportedError,
        0x6700: WrongLengthError,
        0x6985: InvalidStateError,
        0x6735: IncompleteTxnError,
        0x9001: ResendFirstPartError
    }

    def __new__(cls,
                error_code: int,
                ins: Union[int, enum.IntEnum, None] = None,
                message: str = ""
                ) -> Any:
        error_message: str = (f"Error in {ins!r} command"
                              if ins else "Error in command")

        if error_code in DeviceException.exc:
            return DeviceException.exc[error_code](hex(error_code),
                                                   error_message,
                                                   message)

        return UnknownDeviceError(hex(error_code), error_message, message)
