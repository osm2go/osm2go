import QtQuick 2.0
import QtTest 1.0
import "qrc:/"

TestCase {
	id: testCase

	name: "DegreeInput"
	when: windowShown

	property DegreeInput input: null

	Component {
		id: inputFactory
		DegreeInput {
			focus: true
		}
	}
	SystemPalette {
		id: palette
	}

	function init()
	{
		input = inputFactory.createObject(testCase)
	}
	function cleanup()
	{
		input.destroy()
	}

	function shared_data()
	{
		return [
			{ text: "-181", degrees: NaN, valid: false },
			{ text: "-90", degrees: -90, valid: true },
			{ text: "-89", degrees: -89, valid: true },
			{ text: "0", degrees: 0, valid: true },
			{ text: "89", degrees: 89, valid: true },
			{ text: "90", degrees: 90, valid: true },
			{ text: "181", degrees: NaN, valid: false },
			{ text: "a", degrees: NaN, valid: false },
			{ text: "0xa", degrees: NaN, valid: false },
		]
	}

	function shared_check(data)
	{
		input.text = data.text
		compare(input.acceptableInput, data.valid, "wrong acceptableInput for " + data.text + " '" + input.text + "'")

		input.text = ""

		for (var i = 0; i < data.text.length; i++) {
			keyClick(data.text[i])
		}
		keyClick(Qt.Key_Return)

		if (data.valid) {
			compare(input.acceptableInput, data.valid, "wrong acceptableInput for " + data.text + " '" + input.text + "'")
			verify(Qt.colorEqual(input.color, data.valid ? palette.text : "red"), "wrong color " + input.color + " for " + data.text)
			compare(input.degrees, data.degrees, "wrong degrees for " + data.text + " '" + input.text + "'")
		} else {
			if (input.text == data.text)
				compare(input.acceptableInput, false, "entered " + data.text + " was not rejected")
			else if (input.text !== "")
				compare(input.acceptableInput, true, "entered " + data.text + " was not accepted")
			else
				compare(input.acceptableInput, false, "empty text not rejected")
		}

		if (data.valid) {
			input.text = ""
			verify(Qt.colorEqual(input.color, "red"), "wrong color " + input.color + " for empty text")
			compare(input.acceptableInput, false, "empty text not rejected")
			input.text = Number(data.degrees).toLocaleString(Qt.locale(), 'f', 3)
			compare(input.acceptableInput, true, "wrong acceptableInput for " + data.text + " '" + input.text + "'")
			compare(input.color, palette.text, "wrong color " + input.color + " for " + data.text)
			compare(input.degrees, data.degrees, "wrong degrees for " + data.text + " '" + input.text + "'")
		}
	}

	function test_longitude_data()
	{
		return shared_data()
	}

	function test_longitude(data)
	{
		input.latitude = false
		shared_check(data)
	}

	function test_latitude_data()
	{
		var lononly = [
			{ text: "-180", degrees: -180, valid: true },
			{ text: "-179", degrees: -179, valid: true },
			{ text: "179", degrees: 179, valid: true },
			{ text: "180", degrees: 180, valid: true },
		]
		return lononly.concat(shared_data())
	}

	function test_latitude(data)
	{
		input.latitude = true
		shared_check(data)
	}
}
