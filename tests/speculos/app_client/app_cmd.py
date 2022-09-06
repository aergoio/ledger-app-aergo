import struct
from typing import Tuple

from speculos.client import SpeculosClient, ApduResponse, ApduException

from app_client.app_cmd_builder import AppCommandBuilder, InsType
from app_client.exception import DeviceException


class AppCommand:
    def __init__(self,
                 client: SpeculosClient,
                 debug: bool = False) -> None:
        self.client = client
        self.builder = AppCommandBuilder(debug=debug)
        self.debug = debug


    def get_app_and_version(self) -> Tuple[str, str]:
        try:
            response = self.client._apdu_exchange(
                self.builder.get_app_and_version()
            )  # type: int, bytes
        except ApduException as error:
            raise DeviceException(error_code=error.sw, ins=0x01)

        # response = format_id (1) ||
        #            app_name_len (1) ||
        #            app_name (var) ||
        #            version_len (1) ||
        #            version (var) ||
        offset: int = 0

        format_id: int = response[offset]
        offset += 1
        app_name_len: int = response[offset]
        offset += 1
        app_name: str = response[offset:offset + app_name_len].decode("ascii")
        offset += app_name_len
        version_len: int = response[offset]
        offset += 1
        version: str = response[offset:offset + version_len].decode("ascii")
        offset += version_len

        return app_name, version


    def get_version(self) -> Tuple[int, int, int]:
        try:
            response = self.client._apdu_exchange(
                self.builder.get_version()
            )  # type: int, bytes
        except ApduException as error:
            raise DeviceException(error_code=error.sw, ins=InsType.INS_GET_VERSION)

        """
        # response = MAJOR (1) || MINOR (1) || PATCH (1)
        assert len(response) == 3

        major, minor, patch = struct.unpack(
            "BBB",
            response
        )  # type: int, int, int

        return major, minor, patch
        """

        # response = MAJOR (1) || MINOR (1)
        assert len(response) == 2

        major, minor = struct.unpack(
            "BB",
            response
        )  # type: int, int

        return major, minor


    def get_public_key(self, bip32_path: str, display: bool = False) -> bytes:
        try:
            response = self.client._apdu_exchange(
                self.builder.get_public_key(bip32_path=bip32_path,
                                            display=display)
            )  # type: int, bytes
        except ApduException as error:
            raise DeviceException(error_code=error.sw, ins=InsType.INS_GET_PUBLIC_KEY)

        assert len(response) == 33

        return response


    def sign_tx(self, transaction: bytes, model: str) -> Tuple[int, bytes]:
        sw: int
        response: bytes = b""

        chunks = self.builder.get_sign_tx_commands(transaction=transaction)

        #for chunk in chunks:

        #response = self.client._apdu_exchange(chunks[0])
        #print(response)

        client_response = self.client._apdu_exchange_nowait(chunks[0])

        # Review Transaction
        event = self.client.get_next_event()
        print("text :", event["text"])
        event = self.client.get_next_event()
        print("text :", event["text"])
        print("-------------------")

        # Amount
        self.client.press_and_release('right')
        event = self.client.get_next_event()
        print("text :", event["text"])
        event = self.client.get_next_event()
        print("text :", event["text"])
        print("-------------------")

        if model == 'nanos':
            # Recipient
            self.client.press_and_release('right')
            event = self.client.get_next_event()
            print("text :", event["text"])
            event = self.client.get_next_event()
            print("text :", event["text"])
            print("-------------------")

            # Recipient
            self.client.press_and_release('right')
            event = self.client.get_next_event()
            print("text :", event["text"])
            event = self.client.get_next_event()
            print("text :", event["text"])
            print("-------------------")

        # Recipient
        self.client.press_and_release('right')
        event = self.client.get_next_event()
        print("text :", event["text"])
        event = self.client.get_next_event()
        print("text :", event["text"])
        print("-------------------")

        # Recipient
        self.client.press_and_release('right')
        event = self.client.get_next_event()
        print("text :", event["text"])
        event = self.client.get_next_event()
        print("text :", event["text"])
        print("-------------------")

        # Sign Transaction
        self.client.press_and_release('right')
        event = self.client.get_next_event()
        print("text :", event["text"])
        print("-------------------")

        # Approve
        self.client.press_and_release('both')

        response = ApduResponse(client_response).receive()
        print("response :", response)

        assert len(response) > 32

        txn_hash = response[0:32]
        signature = response[32:]

        return txn_hash, signature
