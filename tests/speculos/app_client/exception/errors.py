class UnknownDeviceError(Exception):
    pass


class DenyError(Exception):
    pass


class WrongP1P2Error(Exception):
    pass


class WrongDataLengthError(Exception):
    pass


class WrongLengthError(Exception):
    pass


class InsNotSupportedError(Exception):
    pass


class ClaNotSupportedError(Exception):
    pass


class InvalidStateError(Exception):
    pass


class ResendFirstPartError(Exception):
    pass
