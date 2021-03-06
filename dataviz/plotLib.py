import sys
import csv
import colorsys
from collections import OrderedDict
from subprocess import call
import numpy
import plotly.offline as py
import plotly.graph_objs as go

searchModeNames = {
	0: 'Baseline MCMC',
	1 : 'Randomized MCMC',
	2 : 'Randomized Start k-frontier',
	3 : 'k-frontier MCMC',
	4 : 'CMA-ES'
}

editModeNames = {
	0 : 'Default',
	1 : 'Simple Bandit',
	2 : 'Uniform Random'
}

def getPlots(resultsFile, baseColor, rawTrace = ''):
	threadId = []
	sampleId = []
	time = []
	attrVal = []
	labVal = []
	desc = []
	variance = []
	diameter = []
	minScene = 5000
	minSceneVals = []
	attrMean = 0
	attrMeanVals = []
	startid = 0
	medianVals = []
	pct25 = []
	pct75 = []
	avgTop25 = []

	# gather data
	with open(resultsFile, 'rb') as csvfile:
		freader = csv.reader(csvfile, delimiter=",")
		for row in freader:
			threadId.append(int(row[0]))
			sampleId.append(int(row[1]))
			time.append(float(row[2]))
			attrVal.append(float(row[3]))
			labVal.append(float(row[4]))
			diameter.append(float(row[6]))
			variance.append(float(row[7]))
			desc.append('Score: ' + row[3] + '<br>Display ID:' + str(startid) + ' Thread: ' + row[0] + '<br>Edit History: ' + row[5])

			# minimum attribute scores
			if attrVal[-1] < minScene:
				minScene = attrVal[-1]

			minSceneVals.append(minScene)
			medianVals.append(numpy.median(attrVal))
			pct25.append(numpy.percentile(attrVal, 25))
			pct75.append(numpy.percentile(attrVal, 75))

			# mean attribute score
			attrMean = attrMean + attrVal[-1]
			attrMeanVals.append(attrMean / len(attrVal))

			sortedVals = sorted(attrVal)
			sortedVals = sortedVals[0:25]
			avgTop25.append(sum(sortedVals) / len(sortedVals))

			startid = startid + 1

	events = []
	if rawTrace != '':
		with open(rawTrace, 'rb') as csvfile:
			freader = csv.reader(csvfile, delimiter=',')
			for row in freader:
				if row[5] == "MOVE TARGET":
					resId = int(row[2]) + 1
					events.append(dict(
						time = float(row[0]),
						attrVal = attrVal[resId],
						kind = "Move to Sample " + str(resId),
						thread = int(row[1])
					))
				if row[5] == "FRONTIER ELEMENT":
					resId = int(row[2]) + 1
					events.append(dict(
						time = float(row[0]),
						arrtVal = attrVal[resId],
						kind = "Frontier Element " + str(resId),
						thread = int(row[1])
					))


	# collect events data
	eventText = []
	eventY = []
	eventX = []
	for event in events:
		eventX.append(event['time'])
		eventY.append(event['attrVal'])
		eventText.append(event['kind'] + '<br>Thread: ' + str(event['thread']))

	# fit some curves
	attrLine = numpy.polyfit(time, attrVal, 3)
	labLine = numpy.polyfit(time, labVal, 3)

	f = lambda x: attrLine[0] * x**3 + attrLine[1] * x**2 + attrLine[2] * x + attrLine[3]
	g = lambda x: labLine[0] * x**3 + labLine[1] * x**2 + labLine[2] * x + labLine[3]

	colorStr = 'rgb(' + ','.join(baseColor) + ")"
	innerColorStr = 'rgba(' + ','.join(baseColor) + ", 0.5)"
	outerColorStr = 'rgba(' + ','.join(baseColor) + ", 0.2)"

	# gather plots
	eventPlot = go.Scatter(
		x = eventX,
		y = eventY,
		text = eventText,
		name = 'Events',
		mode = 'markers',
		marker = dict(color = colorStr, size = 10, line=dict(width=1, color='rgb(0,0,0)'))
	)

	attrPoints = go.Scatter(
		x = time,
		y = attrVal,
		mode = 'markers',
		name = 'Attribute Score',
		text = desc,
		marker = dict(color=colorStr)
	)

	varPoints = go.Scatter(
		x = time,
		y = variance,
		yaxis = 'y2',
		mode = 'lines',
		name = "Variance",
		line = dict(color=colorStr)
	)

	diamPoints = go.Scatter(
		x = time,
		y = diameter,
		yaxis = 'y2',
		mode = 'lines',
		name = 'Diameter',
		line = dict(color=colorStr)
	)

	attrLinePlot = go.Scatter(
		x = range(0, int(time[-1])),
		y = map(f, range(0, int(time[-1]))),
		mode = 'lines',
		name = 'trend',
		line = dict(color=colorStr)
	)
	
	# minimum attribute value
	minAttrVal = go.Scatter(
		x = time,
		y = minSceneVals,
		mode = 'lines',
		name = 'Minimum Attribute Value',
		line = dict(color=colorStr)
	)

	# mean attribute value
	meanAttrVal = go.Scatter(
		x = time,
		y = attrMeanVals,
		mode = 'lines',
		name = 'Mean Attribute Value',
		line = dict(color=colorStr)
	)

	# median attribute value
	medianAttrVal = go.Scatter(
		x = time,
		y = medianVals,
		mode = 'lines',
		name = 'Median Attribute Value',
		line = dict(color=colorStr)
	)

	#percentiles
	pct25Plot = go.Scatter(
		x = time,
		y = pct25,
		mode = 'lines',
		name = '25th Percentile',
		line = dict(color=colorStr)
	)

	pct75Plot = go.Scatter(
		x = time,
		y = pct75,
		mode = 'lines',
		name = '75th Percentile',
		line = dict(color=colorStr)
	)

	# error fill
	innerFill = go.Scatter(
		x = time + time[::-1],
		y = pct75 + pct25[::-1],
		mode = 'lines',
		fill = 'tozerox',
		fillcolor = innerColorStr,
		line = dict(color='transparent'),
		name = 'Bounds'
	)

	outerFill = go.Scatter(
		x = time + time[::-1],
		y = pct25 + minSceneVals[::-1],
		mode = 'lines',
		fill = 'tozerox',
		fillcolor = outerColorStr,
		line = dict(color='transparent'),
		name = 'Min Bounds'
	)

	labPoints = go.Scatter(
		x = time,
		y = labVal,
		mode = 'markers',
		name = 'Lab Distance Over Time',
		text = desc,
		marker = dict(color=colorStr)
	)

	labLinePlot = go.Scatter(
		x = range(0, int(time[-1])),
		y = map(g, range(0, int(time[-1]))),
		mode = 'lines',
		name = 'trend',
		line = dict(color=colorStr)
	)

	# Top 25
	top25Plot = go.Scatter(
		x = time,
		y = avgTop25,
		mode = 'lines',
		name = 'Avg 25',
		line = dict(color=colorStr)
	)

	return dict(
		attributePoints = attrPoints,
		variance = varPoints,
		diameter = diamPoints,
		attributeTrend = attrLinePlot,
		minAttrValues = minAttrVal,
		meanAttrValues = meanAttrVal,
		Percentile25 = pct25Plot,
		Percentile50 = medianAttrVal,
		Percentile75 = pct75Plot,
		innerFill = innerFill,
		outerFill = outerFill,
		labPoints = labPoints,
		labTrend = labLinePlot,
		top25 = top25Plot,
		events = eventPlot
	)

def getRawPlots(rawTrace, baseColor):
	threadId = []
	sampleId = []
	time = []
	attrVal = []
	desc = []
	minScene = 5000
	minSceneVals = []
	attrMean = 0
	attrMeanVals = []
	startid = 0
	medianVals = []
	pct25 = []
	pct75 = []
	avgTop25 = []
	events = []

	# gather data
	totalRows = sum(1 for line in open(rawTrace))
	currentRow = 1

	with open(rawTrace, 'rb') as csvfile:
		freader = csv.reader(csvfile, delimiter=",")
		for row in freader:
			if (currentRow % 1000 == 0):
				pct = "{0:>4.0%}".format(float(currentRow) / float(totalRows))
				print "[" + pct + "] Reading Row " + str(currentRow) + " / " + str(totalRows)
			
			threadId.append(int(row[1]))
			sampleId.append(int(row[2]))
			time.append(float(row[0]))
			attrVal.append(float(row[3]))
			desc.append('Score: ' + row[3] + '<br>Display ID:' + str(startid) + ' Thread: ' + row[1] + '<br>Edit: ' + row[5])

			#minSceneVals.append(minScene)
			#medianVals.append(numpy.median(attrVal))
			#pct25.append(numpy.percentile(attrVal, 25))
			#pct75.append(numpy.percentile(attrVal, 75))

			# mean attribute score
			#attrMean = attrMean + attrVal[-1]
			#attrMeanVals.append(attrMean / len(attrVal))

			#sortedVals = sorted(attrVal)
			#sortedVals = sortedVals[0:25]
			#avgTop25.append(sum(sortedVals) / len(sortedVals))

			if row[5] == "MOVE TARGET":
				resId = int(row[2]) + 1
				events.append(dict(
					time = float(row[0]),
					attrVal = attrVal[resId],
					kind = "Move to Sample " + str(resId),
					thread = int(row[1])
				))
			if row[5] == "FRONTIER ELEMENT":
				events.append(dict(
					time = float(row[0]),
					attrVal = float(row[3]),
					kind = "Frontier Element " + row[2],
					thread = int(row[1])
				))

			startid = startid + 1
			currentRow = currentRow + 1

	print "[----] Processing Graph Data..."

	sequentialAttrs = zip(time, attrVal)
	sequentialAttrs = sorted(sequentialAttrs, key = lambda pt: pt[0])

	# calculate stats
	tmpAttrs = []
	currentRow = 1
	for pt in sequentialAttrs:
		if (currentRow % 1000 == 0):
			pct = "{0:>4.0%}".format(float(currentRow) / float(totalRows))
			print "[" + pct + "] Calculating Stats for Point " + str(currentRow) + " / " + str(totalRows)

		tmpAttrs.append(pt[1])
		if pt[1] < minScene:
			minScene = pt[1]

		minSceneVals.append(minScene)
		medianVals.append(numpy.median(tmpAttrs))
		pct25.append(numpy.percentile(tmpAttrs, 25))
		pct75.append(numpy.percentile(tmpAttrs, 75))
		attrMean = attrMean + pt[1]
		attrMeanVals.append(attrMean / len(tmpAttrs))

		sortedVals = sorted(tmpAttrs)
		sortedVals = sortedVals[0:100]
		avgTop25.append(sum(sortedVals) / len(sortedVals))

		currentRow = currentRow + 1

	print "[100%] Collecting Graph Data..."

	# collect events data
	eventText = []
	eventY = []
	eventX = []
	for event in events:
		eventX.append(event['time'])
		eventY.append(event['attrVal'])
		eventText.append(event['kind'] + '<br>Thread: ' + str(event['thread']))

	# fit some curves
	attrLine = numpy.polyfit(time, attrVal, 3)

	f = lambda x: attrLine[0] * x**3 + attrLine[1] * x**2 + attrLine[2] * x + attrLine[3]

	colorStr = 'rgb(' + ','.join(baseColor) + ")"
	innerColorStr = 'rgba(' + ','.join(baseColor) + ", 0.5)"
	outerColorStr = 'rgba(' + ','.join(baseColor) + ", 0.2)"

	# gather plots
	eventPlot = go.Scatter(
		x = eventX,
		y = eventY,
		text = eventText,
		name = 'Events',
		mode = 'markers',
		marker = dict(color = colorStr, size = 10, line=dict(width=1, color='rgb(0,0,0)'))
	)

	attrPoints = go.Scatter(
		x = time,
		y = attrVal,
		mode = 'markers',
		name = 'Attribute Score',
		text = desc,
		marker = dict(color=colorStr)
	)

	attrLinePlot = go.Scatter(
		x = range(0, int(time[-1])),
		y = map(f, range(0, int(time[-1]))),
		mode = 'lines',
		name = 'trend',
		line = dict(color=colorStr)
	)
	
	sortedTime = [x[0] for x in sequentialAttrs]

	# minimum attribute value
	minAttrVal = go.Scatter(
		x = sortedTime,
		y = minSceneVals,
		mode = 'lines',
		name = 'Minimum Attribute Value',
		line = dict(color=colorStr)
	)

	# mean attribute value
	meanAttrVal = go.Scatter(
		x = sortedTime,
		y = attrMeanVals,
		mode = 'lines',
		name = 'Mean Attribute Value',
		line = dict(color=colorStr)
	)

	# median attribute value
	medianAttrVal = go.Scatter(
		x = sortedTime,
		y = medianVals,
		mode = 'lines',
		name = 'Median Attribute Value',
		line = dict(color=colorStr)
	)

	#percentiles
	pct25Plot = go.Scatter(
		x = sortedTime,
		y = pct25,
		mode = 'lines',
		name = '25th Percentile',
		line = dict(color=colorStr)
	)

	pct75Plot = go.Scatter(
		x = sortedTime,
		y = pct75,
		mode = 'lines',
		name = '75th Percentile',
		line = dict(color=colorStr)
	)

	# error fill
	innerFill = go.Scatter(
		x = sortedTime + sortedTime[::-1],
		y = pct75 + pct25[::-1],
		mode = 'lines',
		fill = 'tozerox',
		fillcolor = innerColorStr,
		line = dict(color='transparent'),
		name = 'Bounds'
	)

	outerFill = go.Scatter(
		x = sortedTime + sortedTime[::-1],
		y = pct25 + minSceneVals[::-1],
		mode = 'lines',
		fill = 'tozerox',
		fillcolor = outerColorStr,
		line = dict(color='transparent'),
		name = 'Min Bounds'
	)

	# Top 25
	top25Plot = go.Scatter(
		x = sortedTime,
		y = avgTop25,
		mode = 'lines',
		name = 'Avg 25',
		line = dict(color=colorStr)
	)

	print "[100%] Graph Generation Complete"

	return dict(
		attributePoints = attrPoints,
		attributeTrend = attrLinePlot,
		minAttrValues = minAttrVal,
		meanAttrValues = meanAttrVal,
		Percentile25 = pct25Plot,
		Percentile50 = medianAttrVal,
		Percentile75 = pct75Plot,
		innerFill = innerFill,
		outerFill = outerFill,
		top25 = top25Plot,
		events = eventPlot
	)

def renamePlots(plots, prefix):
	for plot in plots:
		plots[plot]['name'] = prefix + ": " + plots[plot]['name']

	return plots