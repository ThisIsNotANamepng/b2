//This file is part of Bertini 2.
//
//arithmetic.cpp is free software: you can redistribute it and/or modify
//it under the terms of the GNU General Public License as published by
//the Free Software Foundation, either version 3 of the License, or
//(at your option) any later version.
//
//arithmetic.cpp is distributed in the hope that it will be useful,
//but WITHOUT ANY WARRANTY; without even the implied warranty of
//MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//GNU General Public License for more details.
//
//You should have received a copy of the GNU General Public License
//along with arithmetic.cpp.  If not, see <http://www.gnu.org/licenses/>.
//
// Copyright(C) 2015 - 2021 by Bertini2 Development Team
//
// See <http://www.gnu.org/licenses/> for a copy of the license, 
// as well as COPYING.  Bertini2 is provided with permitted 
// additional terms in the b2/licenses/ directory.

// individual authors of this file include:
// silviana amethyst, university of wisconsin eau claire
// Jeb Collins, West Texas A&M


#include "bertini2/function_tree/operators/arithmetic.hpp"




namespace bertini{
	namespace node{
		using ::pow;
		
///////////////////////
//
//  Sum Operator definitions
//
//////////////////////

unsigned SumOperator::EliminateZeros()
{
	assert(!operands_.empty() && "operands_ must not be empty to eliminate zeros");

	unsigned num_eliminated{0};
	if (NumOperands()>1)
	{	
		std::vector<std::shared_ptr<Node>> new_children; std::vector<bool> new_ops;

		std::vector<bool> is_zero(operands_.size(), false);
		for (unsigned ii=0; ii<operands_.size(); ++ii)
			if (operands_[ii]->Eval<dbl>()==0.)	
				is_zero[ii] = true;

		for (unsigned ii=0; ii<NumOperands(); ++ii)
			if (!is_zero[ii])
			{
				new_children.push_back(operands_[ii]);
				new_ops.push_back(signs_[ii]);
			}
			else
			{
				++num_eliminated;
			}

		if (new_children.empty())
		{
			new_children.push_back(Integer::Make(0));
			new_ops.push_back(true);
			--num_eliminated;
		}

		using std::swap;
		swap(operands_, new_children);
		swap(signs_, new_ops);
	}

	// recurse over the remaining children
	for (auto& iter : operands_)
		num_eliminated += iter->EliminateZeros();

	return num_eliminated;
}



unsigned SumOperator::EliminateOnes()
{
	unsigned num_eliminated{0};
	for (auto& iter : operands_)
		num_eliminated += iter->EliminateOnes();

	return num_eliminated;
}

unsigned SumOperator::ReduceSubSums()
{
	std::vector<std::shared_ptr<Node>> new_children;
	std::vector<bool> new_ops;
	unsigned num_eliminated{0};

	for (unsigned ii=0; ii<NumOperands(); ++ii)
	{
		auto converted = std::dynamic_pointer_cast<SumOperator>(operands_[ii]);
		if (converted)
		{ // we have a sum!  reduce it into this one
			for (unsigned jj=0; jj<converted->NumOperands(); ++jj)
			{
				new_children.push_back(converted->operands_[jj]);
				new_ops.push_back(!(converted->signs_[jj] ^ signs_[ii]));
				num_eliminated++;
			}
			
		}
		else
		{
			new_children.push_back(this->operands_[ii]);
			new_ops.push_back(this->signs_[ii]);
		}
	}
	swap(this->operands_, new_children);
	swap(this->signs_, new_ops);
	return num_eliminated;
}


unsigned SumOperator::ReduceSubMults()
{
	std::vector<std::shared_ptr<Node>> new_children;
	std::vector<bool> new_ops;
	unsigned num_eliminated{0};

	for (unsigned ii=0; ii<NumOperands(); ++ii)
	{
		auto converted = std::dynamic_pointer_cast<MultOperator>(operands_[ii]);
		if (converted && converted->NumOperands()==1 && converted->mult_or_div_[0])
		{ // we have a multiply node! if its a single node and is mult, not div, then its operand can be folded into this sum.
			new_children.push_back(converted->operands_[0]);
			new_ops.push_back(signs_[ii]);
			num_eliminated++;			
		}
		else
		{
			new_children.push_back(this->operands_[ii]);
			new_ops.push_back(this->signs_[ii]);
		}
	}

	swap(this->operands_, new_children);
	swap(this->signs_, new_ops);

	return num_eliminated;
}


unsigned SumOperator::ReduceDepth()
{
	auto num_eliminated = ReduceSubSums() + ReduceSubMults();

	for (auto& iter : operands_)
		num_eliminated += iter->ReduceDepth();

	return num_eliminated;
}

void SumOperator::print(std::ostream & target) const
{
	target << "(";
	for (auto iter = operands_.begin(); iter!= operands_.end(); iter++) {
		if (iter==operands_.begin()) {
			// on the first iteration, no need to put a + if a +
			if ( !(*(signs_.begin()+(iter-operands_.begin()))) )
				target << "-";
		}
		else
		{
			if ( !(*(signs_.begin()+(iter-operands_.begin()))) )
				target << "-";
			else
				target << "+";
		}
		(*iter)->print(target);
		
	}
	target << ")";
}


std::shared_ptr<Node> SumOperator::Differentiate(std::shared_ptr<Variable> const& v) const
{
	unsigned int counter = 0;
	std::shared_ptr<Node> ret_sum = Zero();
	for (int ii = 0; ii < operands_.size(); ++ii)
	{
		auto converted = std::dynamic_pointer_cast<Number>(operands_[ii]);
		if (converted)
			continue;
		
		auto temp_node = operands_[ii]->Differentiate(v);
		converted = std::dynamic_pointer_cast<Number>(temp_node);
		if (converted)
			if (converted->Eval<dbl>()==dbl(0.0))
				continue;
		
		counter++;
		if (counter==1)
			ret_sum = SumOperator::Make(temp_node,signs_[ii]);
		else
			std::dynamic_pointer_cast<SumOperator>(ret_sum)->AddOperand(temp_node,signs_[ii]);
		
	}
	
		return ret_sum;
}

int SumOperator::Degree(std::shared_ptr<Variable> const& v) const
{
	int deg = 0;
	
	for (auto iter: operands_)
	{
		auto curr_deg = iter->Degree(v);
		if (curr_deg<0)
			return curr_deg;
		
		deg = std::max(deg, curr_deg);
	}
	return deg;
}

int SumOperator::Degree(VariableGroup const& vars) const 
{
	auto deg = 0;
	for (auto iter = operands_.begin(); iter!=operands_.end(); iter++)
	{
		auto term_degree = (*iter)->Degree(vars);
		if (term_degree<0)
			return term_degree;

		deg = std::max(deg, term_degree);
		
	}
	
	return deg;
}


std::vector<int> SumOperator::MultiDegree(VariableGroup const& vars) const
{
	std::vector<int> deg(vars.size(),0);
	for (auto iter : operands_)
	{
		auto term_deg = iter->MultiDegree(vars);

		for (auto iter = term_deg.begin(); iter!= term_deg.end(); ++iter)
		{
			*(deg.begin()+(iter-term_deg.begin())) = std::max(*(deg.begin()+(iter-term_deg.begin())), *iter);
		}
	}
	return deg;
}

void SumOperator::Homogenize(VariableGroup const& vars, std::shared_ptr<Variable> const& homvar)
{
	
	
	
	// first homogenize each summand.
	for (auto iter: operands_)
	{
		iter->Homogenize(vars, homvar);
	}
	
	// then, homogenize this sum.
	
	// compute the highest degree among all summands.
	int maxdegree = 0;
	std::vector<int> term_degrees;
	// first homogenize each summand.
	for (auto iter: operands_)
	{
		auto local_degree = iter->Degree(vars);
		if (local_degree<0)
			throw std::runtime_error("asking for homogenization on non-polynomial node");
		// TODO: this throw would leave the tree in a partially homogenized state.  this is scary.
		
		term_degrees.push_back(local_degree);
		maxdegree = std::max(maxdegree, local_degree);
	}
	
	for (auto iter = operands_.begin(); iter!=operands_.end(); iter++)
	{
		auto degree_deficiency = maxdegree - *(term_degrees.begin() + (iter-operands_.begin()));
		if ( degree_deficiency > 0)
		{

			// hold the operand temporarily.
			if (degree_deficiency==1)
			{
				std::shared_ptr<Node> M = MultOperator::Make(homvar,std::dynamic_pointer_cast<Node>(*iter));
				swap(*iter,M);
			}
			else{
				std::shared_ptr<Node> P = IntegerPowerOperator::Make(std::dynamic_pointer_cast<Node>(homvar),degree_deficiency);
				std::shared_ptr<Node> M = MultOperator::Make(P,std::dynamic_pointer_cast<Node>(*iter));
				swap(*iter,M);
			}
			

		}
	}
	
}


bool SumOperator::IsHomogeneous(std::shared_ptr<Variable> const& v) const
{
	
	for (auto iter : operands_)
	{
		if (!iter->IsHomogeneous(v))
			return false;
	}

	// the only hope this has of being homogeneous, is that each factor is homogeneous
	int deg;

	deg = (*(operands_.begin()))->Degree(v) ;

	if (deg < 0) 
		return false;

	for (auto iter = operands_.begin()+1; iter!= operands_.end(); iter++)
	{
		auto local_degree = (*iter)->Degree(v);
		if (local_degree!=deg)
			return false;
	}

	return true;
}

bool SumOperator::IsHomogeneous(VariableGroup const& v) const
{
	
	for (auto iter : operands_)
	{
		if (!iter->IsHomogeneous(v))
			return false;
	}

	// the only hope this has of being homogeneous, is that each factor is homogeneous
	int deg;

	deg = (*(operands_.begin()))->Degree(v) ;

	if (deg < 0) 
		return false;

	for (auto iter = operands_.begin()+1; iter!= operands_.end(); iter++)
	{
		auto local_degree = (*iter)->Degree(v);
		if (local_degree!=deg)
			return false;
	}

	return true;
}




dbl SumOperator::FreshEval_d(std::shared_ptr<Variable> const& diff_variable) const
{
	dbl retval;
	this->FreshEval_d(retval, diff_variable);
	return retval;
}
	
void SumOperator::FreshEval_d(dbl& evaluation_value, std::shared_ptr<Variable> const& diff_variable) const
{
	evaluation_value = dbl(0);
	for(int ii = 0; ii < operands_.size(); ++ii)
	{
		if(signs_[ii])
		{
			operands_[ii]->EvalInPlace<dbl>(temp_d_, diff_variable);
			evaluation_value += temp_d_;
		}
		else
		{
			operands_[ii]->EvalInPlace<dbl>(temp_d_, diff_variable);
			evaluation_value -= temp_d_;
		}
	}
}

	
	

mpfr_complex SumOperator::FreshEval_mp(std::shared_ptr<Variable> const& diff_variable) const
{
	mpfr_complex retval;
	this->FreshEval_mp(retval, diff_variable);
	return retval;
}

void SumOperator::FreshEval_mp(mpfr_complex& evaluation_value, std::shared_ptr<Variable> const& diff_variable) const
{
	if (signs_[0])
		operands_[0]->EvalInPlace<mpfr_complex>(evaluation_value, diff_variable);
	else
	{
		operands_[0]->EvalInPlace<mpfr_complex>(temp_mp_, diff_variable);
		evaluation_value = -temp_mp_;
	}

	for(int ii = 1; ii < operands_.size(); ++ii)
	{
		if(signs_[ii])
		{
			operands_[ii]->EvalInPlace<mpfr_complex>(temp_mp_, diff_variable);
			evaluation_value += temp_mp_;
		}
		else
		{
			operands_[ii]->EvalInPlace<mpfr_complex>(temp_mp_, diff_variable);
			evaluation_value -= temp_mp_;
		}
	}
	
}


















//////////////////////
//
//  Negate operator definitions
//
////////////////////////

unsigned NegateOperator::EliminateZeros()
{
	return 0;
}
unsigned NegateOperator::EliminateOnes()
{
	return 0;
}

void NegateOperator::print(std::ostream & target) const
{
	target << "-(";
	operand_->print(target);
	target << ")";
}

std::shared_ptr<Node> NegateOperator::Differentiate(std::shared_ptr<Variable> const& v) const
{
	return NegateOperator::Make(operand_->Differentiate(v));
}

dbl NegateOperator::FreshEval_d(std::shared_ptr<Variable> const& diff_variable) const
{
	return -(operand_->Eval<dbl>(diff_variable));
}

void NegateOperator::FreshEval_d(dbl& evaluation_value, std::shared_ptr<Variable> const& diff_variable) const
{
	operand_->EvalInPlace<dbl>(evaluation_value, diff_variable);
	evaluation_value = -evaluation_value;
}


mpfr_complex NegateOperator::FreshEval_mp(std::shared_ptr<Variable> const& diff_variable) const
{
	return -operand_->Eval<mpfr_complex>(diff_variable);
}

void NegateOperator::FreshEval_mp(mpfr_complex& evaluation_value, std::shared_ptr<Variable> const& diff_variable) const
{
	operand_->EvalInPlace<mpfr_complex>(evaluation_value, diff_variable);
	evaluation_value = -evaluation_value;
}























///////////////////////
//
//  Mult Operator definitions
//
//////////////////////

unsigned MultOperator::EliminateZeros()
{
	assert(!operands_.empty() && "operands_ must not be empty to eliminate zeros");

	// find those zeros in the sum.  then, compress.
	std::vector<bool> non_zeros_ops;

	bool have_a_zero = false;
	for (const auto& iter : operands_)
	{
		if (iter->Eval<dbl>() == 0.)
		{
			have_a_zero = true;
			break;
		}
	}

	if (have_a_zero) // if there is a single zero, the whole thing should collapse.
	{
		unsigned num_eliminated = operands_.size()-1;
		operands_.clear(); mult_or_div_.clear();
		AddOperand(Integer::Make(0), true);
		return num_eliminated;
	}

	// recurse over the remaining children
	unsigned num_eliminated{0};
	for (auto& iter : operands_)
		num_eliminated += iter->EliminateZeros();

	return num_eliminated;
}





unsigned MultOperator::EliminateOnes()
{
	assert(!operands_.empty() && "operands_ must not be empty to eliminate ones");

	unsigned num_eliminated{0};
	if (operands_.size()>1)
	{
		std::vector<bool> is_one(operands_.size(),false);
		

		for (unsigned ii=0; ii<operands_.size(); ++ii)
			is_one[ii] = operands_[ii]->Eval<dbl>()==1.0;

		std::vector<std::shared_ptr<Node>> new_children;
		std::vector<bool> new_mult_div;
		for (unsigned ii=0; ii<is_one.size(); ++ii)
		{
			if (!is_one[ii])
			{
				new_children.push_back(operands_[ii]);
				new_mult_div.push_back(mult_or_div_[ii]);
			}
			else
			{
				++num_eliminated;
			}
		}

		if (new_children.empty())
		{
			new_children.push_back(operands_[0]);
			new_mult_div.push_back(mult_or_div_[0]);
			--num_eliminated;
		}


		using std::swap;
		swap(operands_, new_children);
		swap(mult_or_div_, new_mult_div);
	}

	for (auto& iter : operands_)
		num_eliminated += iter->EliminateOnes();

	return num_eliminated;
}


unsigned MultOperator::ReduceSubSums()
{
	std::vector<std::shared_ptr<Node>> new_children;
	std::vector<bool> new_ops;
	unsigned num_eliminated{0};

	for (unsigned ii=0; ii<NumOperands(); ++ii)
	{
		auto converted = std::dynamic_pointer_cast<SumOperator>(operands_[ii]);
		if (converted && converted->NumOperands()==1)
		{ // we have a sum node! if its a single add node, then its operand can be folded into this sum.
			if (converted->signs_[0])
				new_children.push_back(converted->operands_[0]);
			else
				new_children.push_back(-converted->operands_[0]);

			new_ops.push_back(mult_or_div_[ii]);
			num_eliminated++;
		}
		else
		{
			new_children.push_back(this->operands_[ii]);
			new_ops.push_back(this->mult_or_div_[ii]);
		}
	}

	swap(this->operands_, new_children);
	swap(this->mult_or_div_, new_ops);

	return num_eliminated;	
}

unsigned MultOperator::ReduceSubMults()
{
	std::vector<std::shared_ptr<Node>> new_children;
	std::vector<bool> new_ops;
	unsigned num_eliminated{0};

	for (unsigned ii=0; ii<NumOperands(); ++ii)
	{
		auto converted = std::dynamic_pointer_cast<MultOperator>(operands_[ii]);
		if (converted)
		{ // we have a multiply!  reduce it into this one
			for (unsigned jj=0; jj<converted->NumOperands(); ++jj)
			{
				new_children.push_back(converted->operands_[jj]);
				new_ops.push_back(!(converted->mult_or_div_[jj] ^ this->mult_or_div_[ii]));
				num_eliminated++;
			}
			
		}
		else
		{
			new_children.push_back(this->operands_[ii]);
			new_ops.push_back(this->mult_or_div_[ii]);
		}
	}

	swap(this->operands_, new_children);
	swap(this->mult_or_div_, new_ops);

	return num_eliminated;
}


unsigned MultOperator::ReduceDepth()
{
	auto num_eliminated = ReduceSubSums() + ReduceSubMults();

	for (auto& iter : operands_)
		num_eliminated += iter->ReduceDepth();

	return num_eliminated;
}



void MultOperator::print(std::ostream & target) const
{
	target << "(";
	for (auto iter = operands_.begin(); iter!= operands_.end(); iter++) {
		if (iter==operands_.begin())
			if (! *mult_or_div_.begin()  )
				target << "1/";
		(*iter)->print(target);
		if (iter!=(operands_.end()-1)){
			if (*(mult_or_div_.begin() + (iter-operands_.begin())+1)) { // TODO i think this +1 is wrong... dab
				target << "*";
			}
			else{
				target << "/";
			}
			
		}
	}
	target << ")";
}




std::shared_ptr<Node> MultOperator::Differentiate(std::shared_ptr<Variable> const& v) const
{
	std::shared_ptr<Node> ret_sum = node::Zero();
	
	unsigned term_counter {0};
	// this loop implements the generic product rule, perhaps inefficiently.
	for (int ii = 0; ii < operands_.size(); ++ii)
	{
		auto local_derivative = operands_[ii]->Differentiate(v);
		
		// if the derivative of the current term is 0, then stop 
		auto is_it_a_number = std::dynamic_pointer_cast<Float>(local_derivative);
		if (is_it_a_number)
			if (is_it_a_number->Eval<dbl>()==dbl(0.0))
				continue;
		
		// no, the term's derivative is not 0.  
		
		// create the product of the remaining terms
		auto term_ii = MultOperator::Make(local_derivative);
		for (int jj = 0; jj < operands_.size(); ++jj)
		{
			if(jj != ii)
				term_ii->AddOperand(operands_[jj],mult_or_div_[jj]);
		}
		
		// and then either 1. multiply it against the current derivative, or 2. invoke the quotient rule.
		if (is_it_a_number)
			if (is_it_a_number->Eval<dbl>()==dbl(1.0))
				continue;
		
		// if the derivative of the term under consideration is equal to 1, no need to go on.  already have the product we need.
		
		// if is division, need this for the quotient rule
		if ( !(mult_or_div_[ii]) )
			term_ii->AddOperand(pow(operands_[ii],2),false); // draw a line and square below
		
		term_counter++;
		if (term_counter==1)
			ret_sum = SumOperator::Make(term_ii,mult_or_div_[ii]);
		else
			std::dynamic_pointer_cast<SumOperator>(ret_sum)->AddOperand(term_ii,mult_or_div_[ii]);
	} // re: for ii
	
	return ret_sum;
}

int MultOperator::Degree(std::shared_ptr<Variable> const& v) const
{
	int deg = 0;
	for (auto iter = operands_.begin(); iter!= operands_.end(); iter++)
	{
		// if the operand node is a differential coming from another variable, then this degree should be 0, end of story.
		
		auto factor_deg = (*iter)->Degree(v);
		
		auto is_it_a_differential = std::dynamic_pointer_cast<Differential>(*iter);
		if (is_it_a_differential)
			if (is_it_a_differential->GetVariable()!=v)
			{
				return 0;
			}
		
		
		
		
		
		if (factor_deg<0)
			return factor_deg;
		else if (factor_deg!=0 && !*(mult_or_div_.begin() + (iter-operands_.begin()) ) )
			return -1;
		else
			deg+=factor_deg;
	}
	return deg;
}


int MultOperator::Degree(VariableGroup const& vars) const 
{
	
	auto deg = 0;

	for (auto iter = operands_.begin(); iter!=operands_.end(); iter++)
	{
		auto factor_deg = (*iter)->Degree(vars);  

		if (factor_deg<0)
			return factor_deg;
		else if (factor_deg!=0 && !*(mult_or_div_.begin() + (iter-operands_.begin()) ) )
			return -1;
		else
			deg+=factor_deg;
	}

	
	return deg;
}

std::vector<int> MultOperator::MultiDegree(VariableGroup const& vars) const
{
	std::vector<int> deg(vars.size(),0);
	for (auto iter : operands_)
	{
		auto term_deg = iter->MultiDegree(vars);

		for (auto iter = term_deg.begin(); iter!= term_deg.end(); ++iter)
		{
			*(deg.begin()+(iter-term_deg.begin())) += *iter;
		}
	}
	return deg;
}



void MultOperator::Homogenize(VariableGroup const& vars, std::shared_ptr<Variable> const& homvar)
{
	for (auto iter: operands_)
	{
		iter->Homogenize(vars, homvar);
	}
}


bool MultOperator::IsHomogeneous(std::shared_ptr<Variable> const& v) const
{
	// the only hope this has of being homogeneous, is that each factor is homogeneous
	for (auto iter : operands_)
	{
		if (! iter->IsHomogeneous(v))
		{
			return false;
		}
	}
	return true;
}

bool MultOperator::IsHomogeneous(VariableGroup const& v) const
{
	// the only hope this has of being homogeneous, is that each factor is homogeneous
	for (auto iter : operands_)
	{
		if (! iter->IsHomogeneous(v))
		{
			return false;
		}
	}
	return true;
}

dbl MultOperator::FreshEval_d(std::shared_ptr<Variable> const& diff_variable) const
{
	dbl retval;
	this->FreshEval_d(retval, diff_variable);
	return retval;
}

void MultOperator::FreshEval_d(dbl& evaluation_value, std::shared_ptr<Variable> const& diff_variable) const
{
	evaluation_value = dbl(1);
	for(int ii = 0; ii < operands_.size(); ++ii)
	{
		if(mult_or_div_[ii])
		{
			operands_[ii]->EvalInPlace<dbl>(temp_d_, diff_variable);
			evaluation_value *= temp_d_;
		}
		else
		{
			operands_[ii]->EvalInPlace<dbl>(temp_d_, diff_variable);
			evaluation_value /= temp_d_;
		}
	}
	
}


mpfr_complex MultOperator::FreshEval_mp(std::shared_ptr<Variable> const& diff_variable) const
{
	mpfr_complex retval;
	this->FreshEval_mp(retval, diff_variable);
	return retval;
}

void MultOperator::FreshEval_mp(mpfr_complex& evaluation_value, std::shared_ptr<Variable> const& diff_variable) const
{
	if (mult_or_div_[0])
		operands_[0]->EvalInPlace<mpfr_complex>(evaluation_value, diff_variable);
	else
	{
		operands_[0]->EvalInPlace<mpfr_complex>(temp_mp_, diff_variable);
		evaluation_value = static_cast<mpfr_float>(1)/temp_mp_;
	}

	for(int ii = 1; ii < operands_.size(); ++ii)
	{
		operands_[ii]->EvalInPlace<mpfr_complex>(temp_mp_, diff_variable);
		if(mult_or_div_[ii])
			evaluation_value *= temp_mp_;
		else
			evaluation_value /= temp_mp_;
	}
	
}



////////////
//
//  Power Operator definitions
//
/////////////////


unsigned PowerOperator::EliminateZeros()
{
	return 0;
}
unsigned PowerOperator::EliminateOnes()
{
	return 0;
}


void PowerOperator::Reset() const
{
	Node::ResetStoredValues();
	base_->Reset();
	exponent_->Reset();
}

void PowerOperator::print(std::ostream & target) const
{
	target << "(" << *base_ << ")^(" << *exponent_ << ")";
}


std::shared_ptr<Node> PowerOperator::Differentiate(std::shared_ptr<Variable> const& v) const
{
	
	auto exp_minus_one = exponent_-1;
	auto ret_mult = MultOperator::Make(base_->Differentiate(v));
	ret_mult->AddOperand(exponent_);
	ret_mult->AddOperand(PowerOperator::Make(base_, exp_minus_one));
	return ret_mult;
}


int PowerOperator::Degree(std::shared_ptr<Variable> const& v) const
{
	
	auto base_deg = base_->Degree(v);
	auto exp_deg = exponent_->Degree(v);
	
	if (exp_deg==0)
	{
		auto exp_val = exponent_->Eval<dbl>();
		bool exp_is_int = false;
		
		if (fabs(imag(exp_val))< 10*std::numeric_limits<double>::epsilon()) // so a real thresholding step
			if (fabs(real(exp_val) - std::round(real(exp_val))) < 10*std::numeric_limits<double>::epsilon()) // then check that the real part is close to an integer
				exp_is_int = true;
		
		if (exp_is_int)
		{
			if (abs(exp_val-dbl(0.0))< 10*std::numeric_limits<double>::epsilon())
				return 0;
			else if (real(exp_val)<0)
				return -1;
			else  // positive integer.
			{
				if (base_deg<0)
					return -1;
				else
					return base_deg*std::round(real(exp_val));
			}
			
		}
		else
		{
			if (base_deg==0)
				return 0;
			else
				return -1;
		}
		
	}
	else
	{
		// there may be an edge case here where the base is the number 0 or 1.  but that would be stupid, wouldn't it.
		return -1;
	}
}

int PowerOperator::Degree(VariableGroup const& vars) const
{
	auto multideg = MultiDegree(vars);
	auto deg = 0;
	std::for_each(multideg.begin(),multideg.end(),[&](int n){
					if (n < 0)
						deg = -1;
					else
						deg += n;
					});
	return deg;
}


std::vector<int> PowerOperator::MultiDegree(VariableGroup const& vars) const
{
	std::vector<int> deg(vars.size(),0);
	for (auto iter = vars.begin(); iter!= vars.end(); ++iter)
	{
		*(deg.begin()+(iter-vars.begin())) = this->Degree(*iter);
	}
	return deg;
}



void PowerOperator::Homogenize(VariableGroup const& vars, std::shared_ptr<Variable> const& homvar)
{
	if (exponent_->Degree(vars)==0)
	{
		base_->Homogenize(vars, homvar);
	}
	else{
		throw std::runtime_error("asking for homogenization on non-polynomial node");
		//TODO: this will leave the system in a broken state, partially homogenized...
	}
}

bool PowerOperator::IsHomogeneous(std::shared_ptr<Variable> const& v) const
{
	// the only hope this has of being homogeneous, is that the degree of the exponent is 0 (it's constant), and that it's an integer
	if (exponent_->Degree(v)==0)
	{
		auto exp_val = exponent_->Eval<dbl>();
		if (fabs(imag(exp_val)) < 10*std::numeric_limits<double>::epsilon())
			if (fabs(std::round(real(exp_val)) - real(exp_val)) < 10*std::numeric_limits<double>::epsilon())
				if (real(exp_val) >=0 )
					return base_->IsHomogeneous(v);
	}
	return false;
}


bool PowerOperator::IsHomogeneous(VariableGroup const& v) const
{
	// the only hope this has of being homogeneous, is that the degree of the exponent is 0 (it's constant), and that it's an integer
	if (exponent_->Degree(v)==0)
	{
		auto exp_val = exponent_->Eval<dbl>();
		if (fabs(imag(exp_val)) < 10*std::numeric_limits<double>::epsilon())
			if (fabs(std::round(real(exp_val)) - real(exp_val)) < 10*std::numeric_limits<double>::epsilon())
				if (real(exp_val) >=0 )
					return base_->IsHomogeneous(v);
	}
	return false;
}

dbl PowerOperator::FreshEval_d(std::shared_ptr<Variable> const& diff_variable) const
{
	return std::pow( base_->Eval<dbl>(diff_variable), exponent_->Eval<dbl>());
}

void PowerOperator::FreshEval_d(dbl& evaluation_value, std::shared_ptr<Variable> const& diff_variable) const
{
	dbl temp_d;
	exponent_->EvalInPlace<dbl>(temp_d);
	base_->EvalInPlace<dbl>(evaluation_value, diff_variable);
	
	evaluation_value = std::pow(evaluation_value, temp_d);
}


mpfr_complex PowerOperator::FreshEval_mp(std::shared_ptr<Variable> const& diff_variable) const
{
	return pow( base_->Eval<mpfr_complex>(diff_variable), exponent_->Eval<mpfr_complex>());
}

void PowerOperator::FreshEval_mp(mpfr_complex& evaluation_value, std::shared_ptr<Variable> const& diff_variable) const
{
	mpfr_complex temp_mp;
	exponent_->EvalInPlace<mpfr_complex>(temp_mp);
	base_->EvalInPlace<mpfr_complex>(evaluation_value, diff_variable);
	
	evaluation_value = pow(evaluation_value, temp_mp);
}
















/////////////////
//
//  IntegerPowerOperator definitions
//
////////////////////

unsigned IntegerPowerOperator::EliminateZeros()
{
	return 0;
}
unsigned IntegerPowerOperator::EliminateOnes()
{
	return 0;
}

void IntegerPowerOperator::print(std::ostream & target) const
{
	target << "(";
	operand_->print(target);
	target << "^" << exponent() << ")";
}



std::shared_ptr<Node> IntegerPowerOperator::Differentiate(std::shared_ptr<Variable> const& v) const
{
	if (exponent_==0)
		return Integer::Make(0);
	else if (exponent_==1)
		return operand_->Differentiate(v);
	else if (exponent_==2){
		auto M = MultOperator::Make(Integer::Make(2), operand_);
		M->AddOperand(operand_->Differentiate(v));
		return M;
	}
	else{
		auto M = MultOperator::Make(Integer::Make(exponent_),
												IntegerPowerOperator::Make(operand_, exponent_-1) );
		M->AddOperand(operand_->Differentiate(v));
		return M;
	}
}


int IntegerPowerOperator::Degree(std::shared_ptr<Variable> const& v) const
{
	auto base_deg = operand_->Degree(v);
	if (base_deg<0)
		return base_deg;
	else
		return exponent_*base_deg;
	
}












//////////////
//
//  Square Root Operator definitions
//
/////////////////

unsigned SqrtOperator::EliminateZeros()
{
	return 0;
}
unsigned SqrtOperator::EliminateOnes()
{
	return 0;
}

void SqrtOperator::print(std::ostream & target) const
{
	target << "sqrt(";
	operand_->print(target);
	target << ")";
}



std::shared_ptr<Node> SqrtOperator::Differentiate(std::shared_ptr<Variable> const& v) const
{
	auto ret_mult = MultOperator::Make(PowerOperator::Make(operand_, Rational::Make(mpq_rational(-1,2),0)));
	ret_mult->AddOperand(operand_->Differentiate(v));
	ret_mult->AddOperand(Rational::Make(mpq_rational(1,2),0));
	return ret_mult;
}

int SqrtOperator::Degree(std::shared_ptr<Variable> const& v) const
{
	if (operand_->Degree(v)==0)
	{
		return 0;
	}
	else
	{
		return -1;
	}
}


dbl SqrtOperator::FreshEval_d(std::shared_ptr<Variable> const& diff_variable) const
{
	return sqrt(operand_->Eval<dbl>(diff_variable));
}

void SqrtOperator::FreshEval_d(dbl& evaluation_value, std::shared_ptr<Variable> const& diff_variable) const
{
	operand_->EvalInPlace<dbl>(evaluation_value, diff_variable);
	evaluation_value = sqrt(evaluation_value);
}


mpfr_complex SqrtOperator::FreshEval_mp(std::shared_ptr<Variable> const& diff_variable) const
{
	return sqrt(operand_->Eval<mpfr_complex>(diff_variable));
}

void SqrtOperator::FreshEval_mp(mpfr_complex& evaluation_value, std::shared_ptr<Variable> const& diff_variable) const
{
	operand_->EvalInPlace<mpfr_complex>(evaluation_value, diff_variable);
	evaluation_value = sqrt(evaluation_value);
}















///////////////
//
//  ExpOperator definitions
//
//////////////

unsigned ExpOperator::EliminateZeros()
{
	return 0;
}
unsigned ExpOperator::EliminateOnes()
{
	return 0;
}

void ExpOperator::print(std::ostream & target) const
{
	target << "exp(";
	operand_->print(target);
	target << ")";
}




std::shared_ptr<Node> ExpOperator::Differentiate(std::shared_ptr<Variable> const& v) const
{
	return exp(operand_)*operand_->Differentiate(v);
}


int ExpOperator::Degree(std::shared_ptr<Variable> const& v) const
{
	if (operand_->Degree(v)==0)
	{
		return 0;
	}
	else
	{
		return -1;
	}
}

dbl ExpOperator::FreshEval_d(std::shared_ptr<Variable> const& diff_variable) const
{
	return exp(operand_->Eval<dbl>(diff_variable));
}

void ExpOperator::FreshEval_d(dbl& evaluation_value, std::shared_ptr<Variable> const& diff_variable) const
{
	operand_->EvalInPlace<dbl>(evaluation_value, diff_variable);
	evaluation_value = exp(evaluation_value);
}


mpfr_complex ExpOperator::FreshEval_mp(std::shared_ptr<Variable> const& diff_variable) const
{
	return exp(operand_->Eval<mpfr_complex>(diff_variable));
}

void ExpOperator::FreshEval_mp(mpfr_complex& evaluation_value, std::shared_ptr<Variable> const& diff_variable) const
{
	operand_->EvalInPlace<mpfr_complex>(evaluation_value, diff_variable);
	evaluation_value = exp(evaluation_value);
}











///////////////
//
//  LogOperator definitions
//
//////////////

unsigned LogOperator::EliminateZeros()
{
	return 0;
}
unsigned LogOperator::EliminateOnes()
{
	return 0;
}

void LogOperator::print(std::ostream & target) const
{
	target << "log(";
	operand_->print(target);
	target << ")";
}



std::shared_ptr<Node> LogOperator::Differentiate(std::shared_ptr<Variable> const& v) const
{
	return MultOperator::Make(operand_,false,operand_->Differentiate(v),true);
}


int LogOperator::Degree(std::shared_ptr<Variable> const& v) const
{
	if (operand_->Degree(v)==0)
	{
		return 0;
	}
	else
	{
		return -1;
	}
}

dbl LogOperator::FreshEval_d(std::shared_ptr<Variable> const& diff_variable) const
{
	return log(operand_->Eval<dbl>(diff_variable));
}

void LogOperator::FreshEval_d(dbl& evaluation_value, std::shared_ptr<Variable> const& diff_variable) const
{
	operand_->EvalInPlace<dbl>(evaluation_value, diff_variable);
	evaluation_value = log(evaluation_value);
}


mpfr_complex LogOperator::FreshEval_mp(std::shared_ptr<Variable> const& diff_variable) const
{
	return log(operand_->Eval<mpfr_complex>(diff_variable));
}

void LogOperator::FreshEval_mp(mpfr_complex& evaluation_value, std::shared_ptr<Variable> const& diff_variable) const
{
	operand_->EvalInPlace<mpfr_complex>(evaluation_value, diff_variable);
	evaluation_value = log(evaluation_value);
}


	} // re: namespace node
} // re: namespace bertini
