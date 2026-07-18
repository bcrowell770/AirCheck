module.exports = [
  {
    type: 'heading',
    defaultValue: 'AirCheck Settings'
  },
  {
    type: 'section',
    items: [
      {
        type: 'select',
        messageKey: 'TemperatureUnit',
        label: 'Temperature Units',
        defaultValue: 'f',
        options: [
          { label: 'Fahrenheit', value: 'f' },
          { label: 'Celsius', value: 'c' }
        ]
      },
      {
        type: 'select',
        messageKey: 'AQIScale',
        label: 'AQI Scale',
        defaultValue: 'us',
        options: [
          { label: 'United States (0-500)', value: 'us' },
          { label: 'European', value: 'eu' }
        ]
      }
    ]
  },
  {
    type: 'section',
    items: [
      {
        type: 'select',
        messageKey: 'LocationMode',
        label: 'Location',
        defaultValue: 'phone',
        options: [
          { label: 'Use Phone Location', value: 'phone' },
          { label: 'Use Fixed Location', value: 'fixed' }
        ]
      },
      {
        type: 'input',
        messageKey: 'FixedLocation',
        label: 'City or Postal Code',
        defaultValue: '',
        attributes: {
          placeholder: 'Example: Phoenix or 85001'
        }
      },
      {
        type: 'input',
        messageKey: 'CountryCode',
        label: 'Country Code',
        defaultValue: '',
        attributes: {
          placeholder: 'Example: US'
        }
      },
      {
        type: 'text',
        defaultValue: 'Fixed-location fields are used only when Use Fixed Location is selected.'
      }
    ]
  },
  {
    type: 'submit',
    defaultValue: 'Save'
  }
];
