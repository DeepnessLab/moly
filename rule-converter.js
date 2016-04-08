#!/usr/bin/env node
/*
    The DPI Snort Rule converter works as a command line utility that converts the exported DPI Snort rules to the DPI Service rule format.
    The tool reads the exported rules from an input file and writes the converted rule to an output file.
    For more information run: node rule-converter.js --help
 */

var moment = require('moment');
var program = require('commander');

program
    .version("1.0.0")
    .option('-i, --input <inputFile>', 'DPI Snort rules input file path')
    .option('-o, --output <outputFile>', 'DPI Service rule output folder path')
    .parse(process.argv);

if (!program.inputFile || !program.outputFile) {
    console.log("Input params are not valid. For more information run: node rule-converter.js --help");
    return;
};

console.info("Start rule conversion.");

convertRules(program.inputFile, program.outputFolderPath);

console.info("End rule conversion.");

function convertRules(inputRulePath, outputFolderPath) {
    fs.readFile(inputRulePath, function (err, data) {
        if (err) {
            console.error(err);
            return;
        }

        // Conver the Snort rules to a JSON object.
        var snortRules = JSON.parse(data);
        
    });
};

function writeDpiServiceRules(dpiServiceRules, outputFolderPath) {
	var now = moment.utc();
    var fileName = outputFolderPath + "_" + now;

    var file = fs.createWriteStream(outputFolderPath + "/" + fileName + '.json');
    file.on('error', function(err) {
        console.log("Failed to write the DPI Service rules to output file." + "Error: " + err + ".");
        file.close();
    });

    file.once('open', function(fd) {
        // Write lines to the file with the site name as the first word.
        file.write(siteURL +  " " + words + '\n'); // Write the words that were extracted from the site.

        // Write the URLs extracted from the site.
        urls.forEach(function(url) {
            file.write(siteURL +  " " + url + '\n');
        });

        file.end();
    });
}