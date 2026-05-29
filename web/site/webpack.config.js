const HTMLWebpackPlugin = require('html-webpack-plugin');
const { VueLoaderPlugin } = require('vue-loader');
const path = require('path');
const webPack = require('webpack');

var config = {
  entry: {
	main: './src/main.js',
	management: './src/management.js'
  },
  /*output: {
    path: path.resolve(__dirname, 'dist'),
	filename: 'main.js'
  },*/
  module: {
    rules: [
	  {
	    test: /\.js$/,
		loader: 'babel-loader',
		options: {
		  //presets: [['@babel/preset-env', { "useBuiltIns": 'usage' }]]
	    }
      },
	  {
	    test: /\.vue$/,
		loader: 'vue-loader'
	  },
	  {
	    test: /\.css$/,
		use: [
		  'vue-style-loader',
		  'css-loader'
		]
	  }
	]
  },
  plugins: [ 
    new webPack.IgnorePlugin({ resourceRegExp: /^\.\/locale$/, contextRegExp: /moment$/ }),
    new VueLoaderPlugin(), 
	new HTMLWebpackPlugin({
	  showErrors: true,
	  cache: true,
	  filename: './index.html',
	  template: './index.html',
	  publicPath: '/',
	  chunks: ['main']
	}),
	new HTMLWebpackPlugin({
		showErrors: true,
		cache: true,
		filename: 'management/index.html',
		template: './index.html',
		publicPath: '/',
		chunks: ['management']
	})
  ]
};

module.exports = (env, argv) => {
	if (argv.mode === 'development') {
    config.devtool = 'source-map';
  }
  
  return config;
};