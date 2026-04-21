module.exports = [
  {
    type: 'heading',
    defaultValue: 'AirCheck Settings'
  },
  {
    type: 'text',
    defaultValue: 'Choose the temperature unit used in the app.'
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
          {
            label: 'Fahrenheit',
            value: 'f'
          },
          {
            label: 'Celsius',
            value: 'c'
          }
        ]
      }
    ]
  },
  {
    type: 'submit',
    defaultValue: 'Save'
  }
];
