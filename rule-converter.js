#!/usr/bin/env node
/*
    The DPI Snort Rule converter works as a command line utility that converts the exported DPI Snort rules to the DPI Service rule format.
    The tool reads the exported rules from an input file and writes the converted rule to an output file.
    For more information run: node rule-converter.js --help
 */

var fs = require("fs");
var moment = require('moment');
var program = require('commander');
var path = require('path');

program
    .version("1.0.0")
    .option('-i, --input <input>', 'DPI Snort rules input file path')
    .option('-o, --output <output>', 'DPI Service rule output folder path')
    .parse(process.argv);

if (!program.input || !program.output) {
    console.log("Input params are not valid. For more information run: node rule-converter.js --help");
    return;
}

convertRules(program.input, program.output);

function convertRules(inputRulePath, outputFolderPath) {
    console.info("Start rule conversion.");
    
    fs.readFile(inputRulePath, function (err, data) {
        if (err) {
            console.error(err);
            return;
        }

        // Conver the Snort rules to a JSON object.
        var snortRules = JSON.parse(data);
        writeDpiServiceRules(snortRules.rules, outputFolderPath);
    });
}

function writeDpiServiceRules(dpiServiceRules, outputFolderPath) {
	var now = moment();
    var filename = "dpi_service_rules" + "_" + now.format('MM-DD-YYYY-h.mm.ssa') + '.json';
    var filepath = path.join(outputFolderPath, filename);
    var file = fs.createWriteStream(filepath);
    file.on('error', function(err) {
        console.log("Failed to write the output file." + "Error: " + err + ".");
        file.close();
        console.info("End rule conversion.");
    });

    file.once('open', function(fd) {
        // Write the rules in the format the DPI Service accepts (i.e. fileds without qutation marks and string with a Single quotation mark).
        dpiServiceRules.forEach(function(rule) {
            file.write("{" + "className:\'MatchRule\'" + "," + "rid:" + rule.rid + "," + "pattern:" + "\'" + rule.pattern + "\'" + "," + "is_regex:" + rule.is_regex + "}" + "\n");
        });

        file.end();
        console.info("End rule conversion.");
    });
}

function writeDpiServiceRulesOld(dpiServiceRules, outputFolderPath) {
	var now = moment.utc();
    var filename = "dpi_service_rules" + "_" + now.format('MM-DD-YYYY-h_mm_ssa') + '.json';
    var filepath = path.join(outputFolderPath, filename);
    fs.writeFile(filepath, JSON.stringify(dpiServiceRules),  function(err) {
   		if (err) {
   			console.error(err);
   			return;
   		}
       	
       	console.log("Data written successfully!");
	});
}