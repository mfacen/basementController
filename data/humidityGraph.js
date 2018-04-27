var dataArray = [];

var defaultZoomTime = 24*60*60*1000; // 1 day
var minZoom = -6; // 22 minutes 30 seconds
var maxZoom = 8; // ~ 8.4 months

var zoomLevel = 0;
var viewportEndTime = new Date();
var viewportStartTime = new Date();
//var temperature = 0;
//var extTemperature = 0;
//var output = 0;
//var setpoint = 0;
String.prototype.toInt=function(){
    return parseInt(this.replace(/\D/g, ''),10);
}

//var lastTemperature = 0;

/*					//   THIS IS FOR THE STEEL SERIES
        var sections = [steelseries.Section(0, 25, 'rgba(0, 0, 220, 0.3)'),
                        steelseries.Section(25, 50, 'rgba(0, 220, 0, 0.3)'),
                        steelseries.Section(50, 75, 'rgba(220, 220, 0, 0.3)') ],

            // Define one area
            areas = [steelseries.Section(75, 100, 'rgba(220, 0, 0, 0.3)')],

            // Define value gradient for bargraph
            valGrad = new steelseries.gradientWrapper(  0,
                                                        100,
                                                        [ 0, 0.33, 0.66, 0.85, 1],
                                                        [ new steelseries.rgbaColor(0, 0, 200, 1),
                                                          new steelseries.rgbaColor(0, 200, 0, 1),
                                                          new steelseries.rgbaColor(200, 200, 0, 1),
                                                          new steelseries.rgbaColor(200, 0, 0, 1),
                                                          new steelseries.rgbaColor(200, 0, 0, 1) ]);

var radialTemperature = new steelseries.Radial(document.getElementById('canvasRadialTemperature'), {
                            gaugeType: steelseries.GaugeType.TYPE1,
                            size: 201,
                            section: sections,
                            area: areas,
                            titleString: 'Title',
                            unitString: 'Type1',
                            threshold: 50,
                            lcdVisible: true
                        });
 radialTemperature.setValueAnimated(30);*/

loadCSV(); // Download the CSV data, load Google Charts, parse the data, and draw the chart


/*
Structure:

    loadCSV
        callback:
        parseCSV
        load Google Charts (anonymous)
            callback:
            updateViewport
                displayDate
                drawChart
*/

/*
               |                    CHART                    |
               |                  VIEW PORT                  |
invisible      |                   visible                   |      invisible
---------------|---------------------------------------------|--------------->  time
       viewportStartTime                              viewportEndTime

               |______________viewportWidthTime______________|

viewportWidthTime = 1 day * 2^zoomLevel = viewportEndTime - viewportStartTime
*/

function loadCSV() {
    var xmlhttp = new XMLHttpRequest();
    xmlhttp.onreadystatechange = function() {
        if (this.readyState == 4 && this.status == 200) {
            dataArray = parseCSV(this.responseText);
            google.charts.load('current', { 'packages': ['line', 'corechart'] });
                  google.charts.load('current', {'packages':['gauge']});

            google.charts.setOnLoadCallback(updateViewport);
            //radialTemperature.setValueAnimated(lastTemperature);

        }
    };
    xmlhttp.open("GET", "dataLog.csv", true);
    xmlhttp.send();
    var loadingdiv = document.getElementById("loading");
    loadingdiv.style.visibility = "visible";
}

function parseCSV(string) {
    var array = [];
    var lines = string.split("\n");
    for (var i = 0; i < lines.length; i++) {
        var data = lines[i].split(",", 5);
        data[0] = new Date(parseInt(data[0]) * 1000);
        data[1] = parseFloat(data[1]);
        data[2] = parseFloat(data[2]);
        data[3] = parseInt(data[3]);
        data[4] = parseFloat(data[4]);
        array.push(data);
    }
    return array;
}

function drawChart() {
    var data = new google.visualization.DataTable();
    data.addColumn('datetime', 'UNIX');
    data.addColumn('number', 'Temperature');
    data.addColumn('number', 'ExtTemperature');
    data.addColumn('number', 'Fan');
    data.addColumn('number', 'Output');
    data.addRows(dataArray);

    var options = {
        curveType: 'function',
        displayAnnotations: true,
        height: 360,

        legend: { position: 'bottom' },
        vAxis: {
            viewWindow: {
            min: 0,
            max: 30,
        }
    },



        hAxis: {
            viewWindow: {
                min: viewportStartTime,
                max: viewportEndTime
            },
            gridlines: {
                count: -1,
                units: {
                    days: { format: ['MMM dd'] },
                    hours: { format: ['HH:mm', 'ha'] },
                }
            },
            minorGridlines: {
                units: {
                    hours: { format: ['hh:mm:ss a', 'ha'] },
                    minutes: { format: ['HH:mm a Z', ':mm'] }
                }
            }
        }
    };

    var chart = new google.visualization.LineChart(document.getElementById('chart_div'));

    chart.draw(data,  options);

    var dateselectdiv = document.getElementById("dateselect");
    dateselectdiv.style.visibility = "visible";

    var loadingdiv = document.getElementById("loading");
    loadingdiv.style.visibility = "hidden";
    //console.log(temperature);

        var data1 = google.visualization.arrayToDataTable([
          ['Label', 'Value'],
          ['Temperature', temperature],
          ['Ext-Temperature', extTemperature],
          ['Output', output/4.5],
          ['Setpoint',setpoint]
        ]);

         options = {
          width: 100, height: 400,
          max:40 , min: 0,
          redFrom: 30, redTo: 40,
          yellowFrom:28, yellowTo: 30,
          greenFrom:20, greenTo:30,
          minorTicks: 5
        };

        var gauge = new google.visualization.Gauge(document.getElementById('gauge_div'));

        gauge.draw(data1, options);


        dataFanSpeed = google.visualization.arrayToDataTable([
          ['Label', 'Value'],
          ['Fan Speed', fanSpeed]

        ]);

         options = {
          width: 100, height: 400,
          max:10 , min: 0,
          redFrom: 9, redTo: 10,
          yellowFrom:7, yellowTo: 9,
          greenFrom:0 , greenTo:7,
          minorTicks: 1
        };

         gaugeFan = new google.visualization.Gauge(document.getElementById('gauge_fanSpeed_div'));

        gaugeFan.draw(dataFanSpeed, options);




}  // end of function Draw Chart

function displayDate() { // Display the start and end date on the page
    var dateDiv = document.getElementById("date");

    var endDay = viewportEndTime.getDate();
    var endMonth = viewportEndTime.getMonth();
    var startDay = viewportStartTime.getDate();
    var startMonth = viewportStartTime.getMonth()
    if (endDay == startDay && endMonth == startMonth) {
        dateDiv.textContent = (endDay).toString() + "/" + (endMonth + 1).toString();
    } else {
        dateDiv.textContent = (startDay).toString() + "/" + (startMonth + 1).toString() + " - " + (endDay).toString() + "/" + (endMonth + 1).toString();
    }
}

document.getElementById("prev").onclick = function() {
    viewportEndTime = new Date(viewportEndTime.getTime() - getViewportWidthTime()/3); // move the viewport to the left for one third of its width (e.g. if the viewport width is 3 days, move one day back in time)
    updateViewport();
}
document.getElementById("next").onclick = function() {
    viewportEndTime = new Date(viewportEndTime.getTime() + getViewportWidthTime()/3); // move the viewport to the right for one third of its width (e.g. if the viewport width is 3 days, move one day into the future)
    updateViewport();
}

document.getElementById("zoomout").onclick = function() {
    zoomLevel += 1; // increment the zoom level (zoom out)
    if(zoomLevel > maxZoom) zoomLevel = maxZoom;
    else updateViewport();
}
document.getElementById("zoomin").onclick = function() {
    zoomLevel -= 1; // decrement the zoom level (zoom in)
    if(zoomLevel < minZoom) zoomLevel = minZoom;
    else updateViewport();
}

document.getElementById("reset").onclick = function() {
    viewportEndTime = new Date(); // the end time of the viewport is the current time
    zoomLevel = 0; // reset the zoom level to the default (one day)
    updateViewport();
}
document.getElementById("refresh").onclick = function() {
    viewportEndTime = new Date(); // the end time of the viewport is the current time
    loadCSV(); // download the latest data and re-draw the chart
    console.log("refresh");
    document.getElementById("refresh").bgcolor="cyan";
}

document.body.onresize = drawChart;

function updateViewport() {
    viewportStartTime = new Date(viewportEndTime.getTime() - getViewportWidthTime());
    displayDate();
    drawChart();
}
function getViewportWidthTime() {
    return defaultZoomTime*(2**zoomLevel); // exponential relation between zoom level and zoom time span
                                           // every time you zoom, you double or halve the time scale
}

