import QtQuick 2.0
import QtTest 1.0
import QtPositioning 5.6
import "qrc:/"

TestCase {
	id: testCase

	name: "AreaEdit"
	when: windowShown
	readonly property real minLat: 52.276
	readonly property real minLon: 9.582
	readonly property real maxLat: 52.278
	readonly property real maxLon: 9.585
	// width is ~204m / 0.127mi
	// height is ~222m / 0.138mi
	property variant initialArea: QtPositioning.rectangle(QtPositioning.coordinate(maxLat, minLon), QtPositioning.coordinate(minLat, maxLon))
	property variant initialImperialUnits: 0
	property variant otherBounds: []

	property AreaEdit dialog: null

	Component {
		id: dialogFactory
		AreaEdit {
			focus: true
		}
	}

	function init()
	{
		otherBounds = []
		dialog = dialogFactory.createObject(testCase)
		dialog.initialArea = testCase.initialArea
	}
	function cleanup()
	{
		dialog.destroy()
	}

	function test_positionInit()
	{
		compare(findChild(dialog, "minLat").degrees, minLat)
		compare(findChild(dialog, "minLon").degrees, minLon)
		compare(findChild(dialog, "maxLat").degrees, maxLat)
		compare(findChild(dialog, "maxLon").degrees, maxLon)
		compare(findChild(dialog, "centerLat").degrees, (maxLat + minLat) / 2)
		compare(findChild(dialog, "centerLon").degrees, (maxLon + minLon) / 2)

		// verify that min and max mapping are correct
		verify(Number.fromLocaleString(Qt.locale(), findChild(dialog, "minLat").text) < Number.fromLocaleString(Qt.locale(), findChild(dialog, "centerLat").text))
		verify(Number.fromLocaleString(Qt.locale(), findChild(dialog, "centerLat").text) < Number.fromLocaleString(Qt.locale(), findChild(dialog, "maxLat").text))
		verify(Number.fromLocaleString(Qt.locale(), findChild(dialog, "minLon").text) < Number.fromLocaleString(Qt.locale(), findChild(dialog, "centerLon").text))
		verify(Number.fromLocaleString(Qt.locale(), findChild(dialog, "centerLon").text) < Number.fromLocaleString(Qt.locale(), findChild(dialog, "maxLon").text))
	}

	function test_positionInitUnits()
	{
		fuzzyCompare(Number.fromLocaleString(Qt.locale(), findChild(dialog, "extentWidth").text), 0.204, 0.001)
		fuzzyCompare(Number.fromLocaleString(Qt.locale(), findChild(dialog, "extentHeight").text), 0.222, 0.001)
		dialog.imperialUnits = 1
		fuzzyCompare(Number.fromLocaleString(Qt.locale(), findChild(dialog, "extentWidth").text), 0.127, 0.001)
		fuzzyCompare(Number.fromLocaleString(Qt.locale(), findChild(dialog, "extentHeight").text), 0.138, 0.001)
		dialog.imperialUnits = 0
		fuzzyCompare(Number.fromLocaleString(Qt.locale(), findChild(dialog, "extentWidth").text), 0.204, 0.001)
		fuzzyCompare(Number.fromLocaleString(Qt.locale(), findChild(dialog, "extentHeight").text), 0.222, 0.001)
	}

	function test_changeCenterLat()
	{
		// switch to center tab
		findChild(dialog, "tabbar").currentIndex = 2

		findChild(dialog, "centerLat").text = Number((maxLat + minLat) / 2 + 0.002).toLocaleString(Qt.locale(), 'f', 5)
		findChild(dialog, "centerLat").onEditingFinished()

		// lon should be unchanged
		compare(findChild(dialog, "minLon").degrees, minLon)
		compare(findChild(dialog, "maxLon").degrees, maxLon)
		compare(findChild(dialog, "centerLon").degrees, (maxLon + minLon) / 2)
		compare(dialog.selectedArea.center.longitude, (maxLon + minLon) / 2)

		// lat should have been updated
		fuzzyCompare(findChild(dialog, "minLat").degrees, minLat + 0.002, 0.00001)
		fuzzyCompare(findChild(dialog, "maxLat").degrees, maxLat + 0.002, 0.00001)
		fuzzyCompare(dialog.selectedArea.topLeft.latitude, maxLat + 0.002, 0.00001)
		fuzzyCompare(dialog.selectedArea.bottomRight.latitude, minLat + 0.002, 0.00001)
		fuzzyCompare(dialog.selectedArea.center.latitude, (maxLat + minLat) / 2 + 0.002, 0.00001)
	}

	function test_changeCenterLon()
	{
		// switch to center tab
		findChild(dialog, "tabbar").currentIndex = 2

		findChild(dialog, "centerLon").text = Number((maxLon + minLon) / 2 - 0.003).toLocaleString(Qt.locale(), 'f', 5)
		findChild(dialog, "centerLon").onEditingFinished()

		// lat should be unchanged
		compare(findChild(dialog, "minLat").degrees, minLat)
		compare(findChild(dialog, "maxLat").degrees, maxLat)
		compare(findChild(dialog, "centerLat").degrees, (maxLat + minLat) / 2)
		compare(dialog.selectedArea.center.latitude, (maxLat + minLat) / 2)

		// lon should have been updated
		fuzzyCompare(findChild(dialog, "minLon").degrees, minLon - 0.003, 0.00001)
		fuzzyCompare(findChild(dialog, "maxLon").degrees, maxLon - 0.003, 0.00001)
		fuzzyCompare(dialog.selectedArea.topLeft.longitude, minLon - 0.003, 0.00001)
		fuzzyCompare(dialog.selectedArea.bottomRight.longitude, maxLon - 0.003, 0.00001)
		fuzzyCompare(dialog.selectedArea.center.longitude, (maxLon + minLon) / 2 - 0.003, 0.00001)
	}

	function test_changeWidth_data()
	{
		return [
			{ imperial: 0, widthChange: (400 - 204) / 2 },
			{ imperial: 1, widthChange: (400 / 1.609344 - 204) / 2 }
		]
	}

	function test_changeWidth(data)
	{
		// switch to center tab
		findChild(dialog, "tabbar").currentIndex = 2
		dialog.imperialUnits = data.imperial

		findChild(dialog, "extentWidth").text = Number(0.4).toLocaleString(Qt.locale(), 'f', 5)
		findChild(dialog, "extentWidth").onEditingFinished()

		// lat should be unchanged
		compare(findChild(dialog, "minLat").degrees, minLat)
		compare(findChild(dialog, "maxLat").degrees, maxLat)
		compare(findChild(dialog, "centerLat").degrees, (maxLat + minLat) / 2)

		// center should be unchanged
		compare(dialog.selectedArea.center.longitude, (maxLon + minLon) / 2)
		compare(dialog.selectedArea.center.longitude, (maxLon + minLon) / 2)

		// lon should have been updated

		// verify that min and max mapping are correct
		verify(Number.fromLocaleString(Qt.locale(), findChild(dialog, "minLon").text) < Number.fromLocaleString(Qt.locale(), findChild(dialog, "centerLon").text))
		verify(Number.fromLocaleString(Qt.locale(), findChild(dialog, "centerLon").text) < Number.fromLocaleString(Qt.locale(), findChild(dialog, "maxLon").text))
		verify(Number.fromLocaleString(Qt.locale(), findChild(dialog, "minLon").text) < minLon)
		verify(Number.fromLocaleString(Qt.locale(), findChild(dialog, "maxLon").text) > maxLon)

		fuzzyCompare(findChild(dialog, "minLon").degrees, QtPositioning.coordinate(maxLat, minLon).atDistanceAndAzimuth(data.widthChange, 270).longitude, 0.0015)
		fuzzyCompare(findChild(dialog, "maxLon").degrees, QtPositioning.coordinate(minLat, maxLon).atDistanceAndAzimuth(data.widthChange, 90).longitude, 0.0015)
		fuzzyCompare(dialog.selectedArea.topLeft.longitude, QtPositioning.coordinate(maxLat, minLon).atDistanceAndAzimuth(data.widthChange, 270).longitude, 0.0015)
		fuzzyCompare(dialog.selectedArea.bottomRight.longitude, QtPositioning.coordinate(minLat, maxLon).atDistanceAndAzimuth(data.widthChange, 90).longitude, 0.0015)
	}

	function test_changeHeight_data()
	{
		return [
			{ imperial: 0, heightChange: (400 - 222) / 2 },
			{ imperial: 1, heightChange: (400 / 1.609344 - 222) / 2 }
		]
	}

	function test_changeHeight(data)
	{
		// switch to center tab
		findChild(dialog, "tabbar").currentIndex = 2
		dialog.imperialUnits = data.imperial

		findChild(dialog, "extentHeight").text = Number(0.4).toLocaleString(Qt.locale(), 'f', 5)
		findChild(dialog, "extentHeight").onEditingFinished()

		// lon should be unchanged
		compare(findChild(dialog, "minLon").degrees, minLon)
		compare(findChild(dialog, "maxLon").degrees, maxLon)
		compare(findChild(dialog, "centerLon").degrees, (maxLon + minLon) / 2)

		// center should be unchanged
		compare(dialog.selectedArea.center.longitude, (maxLon + minLon) / 2)
		compare(dialog.selectedArea.center.longitude, (maxLon + minLon) / 2)

		// lat should have been updated

		// verify that min and max mapping are correct
		verify(Number.fromLocaleString(Qt.locale(), findChild(dialog, "minLat").text) < Number.fromLocaleString(Qt.locale(), findChild(dialog, "centerLat").text))
		verify(Number.fromLocaleString(Qt.locale(), findChild(dialog, "centerLat").text) < Number.fromLocaleString(Qt.locale(), findChild(dialog, "maxLat").text))
		verify(Number.fromLocaleString(Qt.locale(), findChild(dialog, "minLat").text) < minLat)
		verify(Number.fromLocaleString(Qt.locale(), findChild(dialog, "maxLat").text) > maxLat)

		fuzzyCompare(findChild(dialog, "minLat").degrees, QtPositioning.coordinate(minLat, maxLon).atDistanceAndAzimuth(data.heightChange, 180).latitude, 0.0015)
		fuzzyCompare(findChild(dialog, "maxLat").degrees, QtPositioning.coordinate(maxLat, minLon).atDistanceAndAzimuth(data.heightChange, 0).latitude, 0.0015)
		fuzzyCompare(dialog.selectedArea.topLeft.latitude, QtPositioning.coordinate(maxLat, minLon).atDistanceAndAzimuth(data.heightChange, 0).latitude, 0.0015)
		fuzzyCompare(dialog.selectedArea.bottomRight.latitude, QtPositioning.coordinate(minLat, maxLon).atDistanceAndAzimuth(data.heightChange, 180).latitude, 0.0015)
	}

	function test_changeMaxLat()
	{
		// switch to center tab
		findChild(dialog, "tabbar").currentIndex = 1

		findChild(dialog, "maxLat").text = Number(maxLat + 0.005).toLocaleString(Qt.locale(), 'f', 5)
		findChild(dialog, "maxLat").onEditingFinished()

		// lon should be unchanged
		compare(findChild(dialog, "minLon").degrees, minLon)
		compare(findChild(dialog, "maxLon").degrees, maxLon)
		compare(findChild(dialog, "centerLon").degrees, (maxLon + minLon) / 2)

		// lat should have been updated
		fuzzyCompare(findChild(dialog, "centerLat").degrees, (maxLat + minLat) / 2 + 0.0025, 0.00001)
		fuzzyCompare(dialog.selectedArea.center.latitude, (maxLat + minLat) / 2 + 0.0025, 0.00001)
	}

	function test_changeMaxLon()
	{
		// switch to center tab
		findChild(dialog, "tabbar").currentIndex = 1

		findChild(dialog, "maxLon").text = Number(maxLon + 0.003).toLocaleString(Qt.locale(), 'f', 5)
		findChild(dialog, "maxLon").onEditingFinished()

		// lat should be unchanged
		compare(findChild(dialog, "minLat").degrees, minLat)
		compare(findChild(dialog, "maxLat").degrees, maxLat)
		compare(findChild(dialog, "centerLat").degrees, (maxLat + minLat) / 2)
		compare(dialog.selectedArea.center.latitude, (maxLat + minLat) / 2)

		// lon should have been updated
		fuzzyCompare(findChild(dialog, "centerLon").degrees, (maxLon + minLon) / 2 + 0.0015, 0.00001)
		fuzzyCompare(dialog.selectedArea.center.longitude, (maxLon + minLon) / 2 + 0.0015, 0.00001)
	}
}
