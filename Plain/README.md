# Plain: Prototype

The prototype of SeedSync includes the client, the storage server, and the synthetic trace generator for evaluation.

## Build

To compile the whole project:

```shell
$ cd ./Plain
$ bash setup.sh # for the first build
```

If the compilation is successful, the executable file is the `bin` folder:

```shell
# currently in the ./SeedSync
$ ls ./bin
sync_config.json  Containers/  Plain  Recipes/
```

`sync_config.json`: the configuration file

`Containers`: the folder to store the container files

`Recipes`: the folder to store the file recipes

`Plain`: the synchronization server of Plain

To re-compile the project and clean store data in the `bin` folder:

```shell
$ cd ./Plain
$ base recompile.sh # clean all store data
```

## Usage

- Before synchronization, you should make sure that the deduplicated storage of the source is not empty (i.e., the /Container /Recipes and db1 is not empty), which means you have run Plain-Dedup with the source machine being the storage server of Plain-Dedup (note: check the README.md under ../Plain-Dedup for details).

- Configuration: you can use `sync_config.json` to configure the system.

```json
{
    "Chunk": {
        "max_chunk_size": 16384,
        "avg_chunk_size": 8192,
        "min_chunk_size": 4096
    },
    "OutsideDB": {
        "out_chunk_db_name": "db1"
    },
    "Cloud_1": {
        "id": 1,
        "ip": "127.0.0.1",
        "p1_send_port": 26666,
        "p3_recv_port": 26667,
        "p3_send_port": 26668,
        "p5_recv_port": 26669,
        "p5_send_port": 26670,
    },
    "Cloud_2": {
        "id": 2,
        "ip": "127.0.0.1",
        "p2_recv_port": 16666,
        "p2_send_port": 16667,
        "p4_recv_port": 16668,
        "p4_send_port": 16669,
        "p6_recv_port": 16670,
    },
    "Sender": {
        "send_data_batch_size": 128,
        "send_meta_batch_size": 1024
    },
    "EnclaveCache": {
        "enclave_cache_item": 512
    }
}
```

Note that you need to modify `ip` of "Cloud-1" and "Cloud-2" according to the machines that run the Plain server.

- SeedSync usage: 

```shell
$ cd ./Plain
$ ./Plain -t [s/w] -i [inputFile path] 
```

`-t`: operation type, s: the source issues a **s**ynchronization request; w: the destination **w**aits for a synchronization request

`-i`: input file path

After each run, the source will record the running result in `Src-Log` in the `bin` folder, while the destination will record the running result in `Dest-Log` in the `bin` folder.

Note that you can use "ctrl + c" to close the destination when it is idle, while the source will exit after finish the synchronization.
