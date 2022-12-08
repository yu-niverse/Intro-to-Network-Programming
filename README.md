# Introduction to Network Programming

## Environment Setup - Docker

Install Docker & Check Installation
```
$ docker -v
```

Pull Demo Image
```
$ docker pull guanxq0927/nplinux_arm:v1
```
Check Images
```
$ docker images
```

Create Container: Map Host File to Container
```
$ docker run -it -v "your host file path":"container file path" imagename bin/bash
```

Connect to the Container
```
$ docker exec -it "container_name" bash
```
