#!/usr/bin/env python3
import socket, argparse

def main():
    p = argparse.ArgumentParser()
    p.add_argument("--listen-host", default="127.0.0.1")
    p.add_argument("--listen-port", type=int, default=7777)
    args = p.parse_args()

    srv = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    srv.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    srv.bind((args.listen_host, args.listen_port))
    srv.listen(1)
    print(f"Dummy GS-232 logger on {args.listen_host}:{args.listen_port}")

    while True:
        conn, addr = srv.accept()
        print("GPredict connected from", addr)
        with conn:
            while True:
                data = conn.recv(256)
                if not data:
                    break
                cmd = data.decode("ascii", errors="ignore").strip()
                print("→ GPredict:", repr(cmd))

                # fake values
                az = "179.9"
                el = "53.8"

                if cmd == 'p':
                    resp = az
                elif cmd == 'P':
                    resp = f"P{az} {el}"
                elif cmd.upper() == 'S':
                    resp = "S0"
                elif cmd.upper() == 'Q':
                    # no reply on Q, just break out
                    print("← Dummy: <closing>")
                    break
                else:
                    # for AZ/EL set commands and others we just acknowledge
                    resp = cmd.upper()

                print("← Dummy:", repr(resp))
                conn.sendall((resp + "\r\n").encode("ascii"))

        print("GPredict disconnected")

if __name__ == "__main__":
    main()
