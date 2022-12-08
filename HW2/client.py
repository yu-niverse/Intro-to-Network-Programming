import select
import socket
import pathlib
from time import sleep
import argparse

BUFFER_SIZE = 1024
SELECT_TIMEOUT = 0.001
SLEEP_FOR_SOCKET_READY = 0.025
SERVER_ADDRESS = 'localhost'


class Client:

    def __init__(self, n, addr, port) -> None:
        assert isinstance(n, int)

        self.tcp_socket_table = {
            client_id: socket.create_connection((addr, port))
            for client_id in range(n)
        }

        self.udp_socket = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        self.server_port = port

    def __del__(self):
        for _, tcp_socket in self.tcp_socket_table.items():
            tcp_socket.close()

        self.udp_socket.close()

    def send_and_recv_udp_socket(self, message: str, bufsize=BUFFER_SIZE):
        self.udp_socket.sendto(message.encode('ascii'),
                               (SERVER_ADDRESS, self.server_port))
        data, _ = self.udp_socket.recvfrom(bufsize)
        return data.decode('ascii')


class ClientCommand:

    def __init__(self, client_id, socket_type, command) -> None:
        self._client_id = client_id
        self._socket_type = socket_type
        self._command = command

    @property
    def client_id(self):
        return self._client_id

    @property
    def socket_type(self):
        return self._socket_type

    @property
    def command(self):
        return self._command


if __name__ == "__main__":
    parser = argparse.ArgumentParser('Client script')

    parser.add_argument('-p', dest='port', type=int, default=8888)
    parser.add_argument('--src', dest='src_dir', type=str, default='testcases')
    parser.add_argument('--filename', dest='filename', type=str, default='')
    parser.add_argument('--dst',
                        dest='dst_dir',
                        type=str,
                        default='testcases_correct')

    args = parser.parse_args()

    n = None
    commands = list()
    src_dir = args.src_dir
    filename = args.filename
    dst_dir = args.dst_dir
    filepath = f'{src_dir}/{filename}'
    out_filepath = f'{dst_dir}/{filename}'

    pathlib.Path(dst_dir).mkdir(parents=True, exist_ok=True)

    with open(filepath, 'r', encoding='utf-8') as f:
        lines = f.readlines()
        n = int(lines[0])
        for line in lines[1:]:
            tokens = ' '.join(line.split()).split(' ', 2)
            assert len(tokens) == 3
            assert tokens[1] in ['tcp', 'udp', 'special']
            commands.append(ClientCommand(int(tokens[0]), tokens[1],
                                          tokens[2]))

    client = Client(n, SERVER_ADDRESS, args.port)
    with open(out_filepath, 'w+', encoding='utf-8') as out_f:
        for cmd in commands:
            # Special command, exit client, the same as sending EOF to server
            if cmd.command == 'exit':
                sock = client.tcp_socket_table.get(cmd.client_id)
                assert sock
                sock.close()
                client.tcp_socket_table.pop(cmd.client_id)
                continue

            if cmd.socket_type == 'tcp':
                tcp_sock = client.tcp_socket_table[cmd.client_id]
                message = cmd.command.encode('ascii') + b'\n'
                tcp_sock.send(message)
                data = tcp_sock.recv(BUFFER_SIZE)
                assert data
                data = data.decode('ascii')
                out_f.write(f'({cmd.client_id}) {data}')

            elif cmd.socket_type == 'udp':
                data = client.send_and_recv_udp_socket(cmd.command)
                out_f.write(f'({cmd.client_id}) {data}')

            inputs = list(client.tcp_socket_table.values())

            sleep(SLEEP_FOR_SOCKET_READY)
            readable_sockets, _, _ = select.select(inputs, list(), list(),
                                                   SELECT_TIMEOUT)

            for client_id, tcp_sock in client.tcp_socket_table.items():
                if tcp_sock in readable_sockets:
                    data = tcp_sock.recv(BUFFER_SIZE)
                    assert data
                    data = data.decode('ascii')
                    out_f.write(f'(Broadcast to {client_id}) {data}')