#!/usr/bin/python
#
# Some Attributes derived from the Eigenvalues of the Orientation Tensor
#
# Calculates a series os attributes based on the eigenvalues of an orientation tensor
#
# Cline = (e2-e3)/(e2+e3) (Bakker, 2001)
# Cplane = (e1-e2)/(e1+e2) (Bakker, 2001)
# Cfault = Cline * (1 - Cplane) (Bakker, 2001)
# Cchaos = 4*e1*e3*(e1+e2+e3)/(e1+e3)^2 (Wang etal, 2009)
# Ctype = ((e1-e2)^2 + (e1-e3)^2 + (e2-e3)^2) / (e1^2 + e2^2 + e3^2) (Haubecker and Jahne, 1996)
#
# The orientation tensor is the gradient energy tensor using Scharr's 3 point derivative operator.
# 
# The eigenvalues are numbered in decreasing order of their magnitude.
#
import sys
import numpy as np
import scipy.ndimage as ndi
#
# Import the module with the I/O scaffolding of the External Attribute
#
import extattrib as xa

#
# These are the attribute parameters
#
xa.params = {
	'Input': 'Input',
	'Output': ['Cline', 'Cplane', 'Cfault', 'Cchaos', 'Ctype'],
	'ZSampMargin' : {'Value':[-2,2], 'Hidden': True},
	'StepOut' : {'Value': [2,2], 'Hidden': True},
	'Help': 'http://waynegm.github.io/OpendTect-Plugin-Docs/External-Attributes/GET-Attributes/'
}
#
# Define the compute function
#
def doCompute():
	hxs = xa.SI['nrinl']//2
	hys = xa.SI['nrcrl']//2
	while True:
		xa.doInput()

		gx = scharr3(xa.Input, axis=0)
		gy = scharr3(xa.Input, axis=1)
		gz = scharr3(xa.Input, axis=2)

		gxx = scharr3(gx, axis=0)
		gyy = scharr3(gy, axis=1)
		gzz = scharr3(gz, axis=2)
		gxy = scharr3(gy, axis=0)
		gxz = scharr3(gz, axis=0)
		gyz = scharr3(gz, axis=1)

		laplace = gxx + gyy + gzz
		gx3 = scharr3(laplace,axis=0)
		gy3 = scharr3(laplace,axis=1)
		gz3 = scharr3(laplace,axis=2)

		H = np.array([	[gxx[hxs,hys,:],gxy[hxs,hys,:],gxz[hxs,hys,:]],
						[gxy[hxs,hys,:],gyy[hxs,hys,:],gyz[hxs,hys,:]],
						[gxz[hxs,hys,:],gyz[hxs,hys,:],gzz[hxs,hys,:]]])
		dG = np.array([[gx[hxs,hys,:]],[gy[hxs,hys,:]],[gz[hxs,hys,:]]])
		Tg = np.array([[gx3[hxs,hys,:]],[gy3[hxs,hys,:]],[gz3[hxs,hys,:]]])

		T = np.zeros((3,3))
		res = np.zeros((3,xa.Input.shape[2]))
		for i in range(xa.Input.shape[2]):
			T = H[:,:,i].dot(H[:,:,i].T) - 0.5 * (dG[:,:,i].dot(Tg[:,:,i].T) + Tg[:,:,i].dot(dG[:,:,i].T))
			evals = np.sort(np.linalg.eigvalsh(T))
			res[:,i] = evals

		e1 = res[2,:]
		e2 = res[1,:]
		e3 = res[0,:]
		e1me2 = e1-e2
		e1me3 = e1-e3
		e2me3 = e2-e3
		e1pe3 = e1+e3
		xa.Output['Cline'] = e2me3/(e2+e3)
		xa.Output['Cplane'] = e1me2/(e1+e2)
		xa.Output['Cfault'] = xa.Output['Cline']*(1.0 - xa.Output['Cplane'])
		xa.Output['Cchaos'] = 4.0 * e1 * e3 * (e1 + e2 + e3)/(e1pe3*e1pe3)
		xa.Output['Ctype'] = (e1me2*e1me2 + e1me3*e1me3 + e2me3*e2me3)/(e1*e1 + e2*e2 + e3*e3)

		xa.doOutput()
	
#
# Scharr 3 pont derivative filter
#
def scharr3( input, axis=-1, output=None, mode="reflect", cval=0.0):
	"""Calculate a size 3 Scharr derivative filter.
	Parameters
	----------
	%(input)s
	%(axis)s
	%(output)s
	%(mode)s
	%(cval)s
	"""
	input = np.asarray(input)
	axis = ndi._ni_support._check_axis(axis, input.ndim)
	output, return_value = ndi._ni_support._get_output(output, input)
	ndi.correlate1d(input, [-0.5, 0, 0.5], axis, output, mode, cval, 0)
	axes = [ii for ii in range(input.ndim) if ii != axis]
	for ii in axes:
		ndi.correlate1d(output, [0.12026,0.75948,0.12026], ii, output, mode, cval, 0,)
	return return_value
#
# Assign the compute function to the attribute
#
xa.doCompute = doCompute
#
# Do it
#
xa.run(sys.argv[1:])
  
