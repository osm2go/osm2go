import QtQuick 2.0
import QtQuick.Controls 2.0

TextField {
	id: textfield

	property real degrees
	property bool latitude
	readonly property real limit: latitude ? 180 : 90

	SystemPalette {
		id: palette
	}

	text: Number(degrees).toLocaleString(Qt.locale(), 'f', 5)
	validator: DoubleValidator {
		bottom: -textfield.limit
		top: textfield.limit
		decimals: 8
		notation: DoubleValidator.StandardNotation
	}
	color: acceptableInput ? palette.text : "red"
	onEditingFinished: degrees = acceptableInput ? Number.fromLocaleString(Qt.locale(), text) : NaN
}
