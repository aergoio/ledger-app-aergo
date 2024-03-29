import pytest

from speculos.client import ApduException

from app_client.exception import *


@pytest.mark.xfail(raises=ClaNotSupportedError)
def test_bad_cla(client):
    try:
        client.apdu_exchange(cla=0xA0,  # 0xA0 instead of 0xAE
                             ins=0x03)
    except ApduException as error:
        raise DeviceException(error_code=error.sw)


@pytest.mark.xfail(raises=InsNotSupportedError)
def test_bad_ins(client):
    try:
        client.apdu_exchange(cla=0xAE,
                             ins=0x1F)  # bad INS
    except ApduException as error:
        raise DeviceException(error_code=error.sw)


@pytest.mark.xfail(raises=WrongDataLengthError)
def test_wrong_data_length(client):
    try:
        # APDUs must be at least 5 bytes: CLA, INS, P1, P2, Lc.
        client._apdu_exchange(b"AE00")
    except ApduException as error:
        raise DeviceException(error_code=error.sw)


@pytest.mark.xfail(raises=WrongDataLengthError)
def test_wrong_lc(client):
    try:
        client._apdu_exchange(b"AE0003000011")
    except ApduException as error:
        raise DeviceException(error_code=error.sw)
