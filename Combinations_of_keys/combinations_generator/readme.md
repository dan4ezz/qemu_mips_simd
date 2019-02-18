### Build project

## Compile
sbt compile

## Build jar
sbt assembly

## Usage
- Download keys.txt and combination-generator.jar to your workstation
- execute:
java -jar combination-generator.jar <file_with_keys> <output_file>

Example:
java -jar combination-generator.jar keys.txt result.txt

Also you are able to use absolute paths.