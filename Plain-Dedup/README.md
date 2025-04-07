# Plain-Dedup: Prototype
The prototype of Plain-Dedup includes the client, and the storage server. It is modified from DEBE's prototype by disabling encryption. 

Note that this prototype is only used for generating container data and metadata (i.e., indexes, file recipes) of a plaintext deduplicated backend, which is used for evaluating the plaintext synchronization (Plain). The deduplication performance is orthogonal to our work of data synchronization.

## Build
To compile the whole project:

```shell
$ cd ./Plain-Dedup
$ bash setup.sh # for the first build
```

If the compilation is successful, the executable file is the `bin` folder (note that we didn't rename the execution file name from DEBE, yet the functionality is without encryption):

```shell
# currently in the ./DEBE/Prototype
$ ls ./bin
config.json  Containers/  DEBEClient*  DEBEServer*  Recipes/
```

`config.json`: the configuration file

`Containers`: the folder to store the container files

`Recipes`: the folder to store the file recipes

`DEBEClient`: the client

`DEBEServer`: the storage server

To re-compile the project and clean store data in the `bin` folder:

```shell
$ cd ./Plain-Dedup
$ bash recompile.sh # clean all store data
```

Note that `recompile.sh` will copy `./Plain-Dedup/config.json` to `./Plain-Dedup/bin` for re-configuration. Please ensure that `config.json` in `./Plain-Dedup/bin` is correct in your configuration (You can edit the `./Plain-Dedup/config.json` to set up your configuration and run `recompile.sh` to validate your configuration). 

## Usage

- Configuration: you can use `config.json` to configure the system.

```json
{   
    "ChunkerConfig":{
        "chunkingType_": 1, // chunking type: 1: FastCDC
        "maxChunkSize_": 16384, // max chunk size
        "minChunkSize_": 4096, // avg chunk size
        "avgChunkSize_": 8192, // min chunk size
        "slidingWinSize_": 128, // chunking sliding window size
        "readSize_": 128 // read data buffer size
    },
    "StorageCore": {
        "recipeRootPath_": "Recipes/", // the recipe path
        "containerRootPath_": "Containers/", // the container path
        "fp2ChunkDBName_": "db1", // the name of the index file
        "topKParam_": 512 // the size of top-k index, unit (K, 1024)
    },
    "RestoreWriter": {
        "readCacheSize_": 64 // the restore container cache size
    },
    "DataSender": {
        "storageServerIp_": "192.168.11.214", // the storage server ip (need to modify)
        "storageServerPort_": 16666, // the storage server port (need to modify)
        "clientID_": 1, // the id of the client (can be modify)
        "localSecret_": "12345", // the client master key
        "sendChunkBatchSize_": 128, // the batch size of sending chunks
        "sendRecipeBatchSize_": 1024, // the batch size of sending key recipes
        "spid_": "259A7E2BC521D75621AEA63669BEA34D", // remote attestation setting
        "quoteType_": 0, // remote attestation setting
        "iasServerType_": 0, // remote attestation setting
        "iasPrimaryKey_": "fee17e94cd834ec7a3ed4e72bf04f795", // remote attestation setting
        "iasSecKey_": "0223f86f98154b6b9316054658eda2d3", // remote attestation setting
        "iasVersion_": 4 // remote attestation setting
    }
}
```

Note that you need to modify `storageServerIp_`, and `storageServerPort_` according to the machines that run the storage server.

- Client usage: 

```shell
$ cd ./Plain-Dedup/bin
# u: upload; d: download
$ ./DEBEClient -t [u/d] -i [inputFile path]
```

`-t`: operation type, upload/download

`-i`: input file path

After each run, the client will record the running result in `client-log` in the `bin` folder.

- Storage server usage:

```shell
$ cd ./Plain-Dedup/bin
# we follow the default setting in DEBE
$ ./DEBEServer -m 4
```

Note that you can use "ctrl + c" to close the storage server when it is idle.
