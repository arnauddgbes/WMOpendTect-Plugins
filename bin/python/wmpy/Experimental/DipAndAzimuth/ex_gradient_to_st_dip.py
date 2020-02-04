#
# Calculate Orientation attributes from pre-calculated gradients using the Gradient Structure Tensor (GST)
# Inputs: Inline, Crossline and Z gradients
# Outputs: Inline, Crossline, True dip, Dip Azimuth and GST Coherency/Saliency
#
import sys,os
import numpy as np
#
# Import the module with the I/O scaffolding of the External Attribute
#
sys.path.insert(0, os.path.join(sys.path[0], '..','..'))
import extattrib as xa
import extlib as xl
#
# These are the attribute parameters
#
xa.params = {
	'Inputs': ['In-line gradient', 'Cross-line gradient', 'Z gradient'],
	'Output': ['Crl_dip', 'Inl_dip', 'True Dip', 'Dip Azimuth', 'Coherency'],
	'ZSampMargin' : {'Value':[-3,3], 'Symmetric': True},
	'StepOut' : {'Value': [3,3], 'Symmetric': True}
}
#
# Define the compute function
#
def doCompute():
	xs = xa.SI['nrinl']
	ys = xa.SI['nrcrl']
	zs = xa.params['ZSampMargin']['Value'][1] - xa.params['ZSampMargin']['Value'][0] + 1
	kernel = xl.getGaussian(xs, ys, zs)
	inlFactor = xa.SI['zstep']/xa.SI['inldist'] * xa.SI['dipFactor']
	crlFactor = xa.SI['zstep']/xa.SI['crldist'] * xa.SI['dipFactor']
	while True:
		xa.doInput()

		gx = xa.Input['In-line gradient']
		gy = xa.Input['Cross-line gradient']
		gz = xa.Input['Z gradient']
#
#	Inner product of  gradients
		gx2 = gx * gx
		gy2 = gy * gy
		gz2 = gz * gz
		gxgy = gx * gy
		gxgz = gx * gz
		gygz = gy * gz
#
#	Outer gaussian smoothing
		rgx2 = xl.sconvolve(gx2, kernel)
		rgy2 = xl.sconvolve(gy2, kernel)
		rgz2 = xl.sconvolve(gz2, kernel)
		rgxgy = xl.sconvolve(gxgy, kernel)
		rgxgz = xl.sconvolve(gxgz, kernel)
		rgygz = xl.sconvolve(gygz, kernel)
#
#	Form the structure tensor
		T = np.rollaxis(np.array([	[rgx2,  rgxgy, rgxgz],
									[rgxgy, rgy2,  rgygz],
									[rgxgz, rgygz, rgz2 ]]), 2)
#
#	Get the eigenvalues and eigen vectors and calculate the dips
		evals, evecs = np.linalg.eigh(T)
		ndx = evals.argsort()
		evecs = evecs[np.arange(0,T.shape[0],1),:,ndx[:,2]]
		eval2 = evals[np.arange(0,T.shape[0],1),ndx[:,2]]
		eval1 = evals[np.arange(0,T.shape[0],1),ndx[:,1]]
		xa.Output['Crl_dip'] = -evecs[:,1]/evecs[:,2]*crlFactor
		xa.Output['Inl_dip'] = -evecs[:,0]/evecs[:,2]*inlFactor
		xa.Output['True Dip'] = np.sqrt(xa.Output['Crl_dip']*xa.Output['Crl_dip']+xa.Output['Inl_dip']*xa.Output['Inl_dip'])
		xa.Output['Dip Azimuth'] = np.degrees(np.arctan2(xa.Output['Inl_dip'],xa.Output['Crl_dip']))
		coh = (eval2-eval1)/(eval2+eval1) 
		xa.Output['Coherency'] = coh * coh
		xa.doOutput()
	
#
# Assign the compute function to the attribute
#
xa.doCompute = doCompute
#
# Do it
#
xa.run(sys.argv[1:])
  
