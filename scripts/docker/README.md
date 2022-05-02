## Docker stack for self hosted GH runner

### Usage

This directory contains a docker image and basic compose file to 
setup a linux Github self-hosted actions runner.

This is not an ephemeral container, the container will stay running 
untill stopped, or an error occurs. Credentials are persisted by 
way of a named volume, so will survive container removal.

To use with docker-compose, copy `.env.template` to `.env`, and 
populate `RUNNER_REG_TOKEN`. This token will be supplied solely at 
the discretion of the dosbox-staging repository admins, and is valid  
for 60 minutes.

Copy `docker-compose.yml.template` to `docker-compose.yml`, and 
modify `hostname` keys to your own username. Additionally, you may 
add or remove the number of services to suit the resources you 
have available.

Start the stack with `docker-compose up -d`. You can follow the logs 
with `docker-compose logs -f`. You can stop everything with 
`docker-compose down`

### Building

To build the image, you can invoke the following from the 
`scripts/docker` directory:

```
DOCKER_BUILDKIT=1 docker build -t ghcr.io/dosbox-staging/self-gh-runner:latest .
```
Note that `DOCKER_BUILDKIT=1` is required.

Alternatively, [buildx](https://docs.docker.com/buildx/working-with-buildx/) can 
be used instead.
