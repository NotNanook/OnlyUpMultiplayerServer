# Use a base image with the desired operating system and dependencies
FROM ubuntu:latest

# Set the working directory inside the container
WORKDIR /app

# Copy the source code into the container
COPY server.cpp .

# Install any necessary dependencies
RUN apt-get update && \
    apt-get install -y g++

# Compile the source code
RUN g++ -pthread server.cpp -o server

# Specify the command to run when the container starts
CMD ["./server"]