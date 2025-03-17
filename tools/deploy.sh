#!/bin/bash

set -e 

# Install Docker and start it. Change this if your server has already
# Docker installed or doesn't use yum and systemctl.
sudo yum install -y docker
sudo systemctl start docker

# Remove previous containers and networks
sudo docker stop redis || echo "Redis container not running"
sudo docker stop mysql || echo "MySQL container not running"
sudo docker stop cppserver || echo "cppserver container not running"
sudo docker container prune -f
sudo docker network rm chat-net || echo "Network does not exist"

# Run our application
sudo docker run -d \
    --name redis \
    -v /data/redis-data:/data redis:alpine \
    redis-server \
    --appendonly yes
sudo docker run -d \
    --name mysql \
    -v /data/mysql-data:/var/lib/mysql \
    -v $HOME/deploy/db_setup.sql:/docker-entrypoint-initdb.d/db_setup.sql \
    -e MYSQL_ROOT_PASSWORD=$MYSQL_ROOT_PASSWORD \
    -e MYSQL_PASSWORD=$MYSQL_PASSWORD \
    mysql:9.2.0
sudo docker run -d \
    --name cppserver \
    -p 80:80 \
    -e REDIS_HOST=redis \
    -e MYSQL_HOST=mysql \
    -e MYSQL_PASSWORD=$MYSQL_PASSWORD \
    $1
sudo docker network create chat-net
sudo docker network connect chat-net redis
sudo docker network connect chat-net mysql
sudo docker network connect chat-net cppserver

# Remove unused images
sudo docker image prune -a -f
